#!/usr/bin/env python3
"""Train and export a small quantized MNIST CNN for the SystemC PE array."""

import argparse
import gzip
import math
import struct
import urllib.request
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
RAW_DIR = ROOT / "data" / "mnist" / "raw"
OUTPUT_DIR = ROOT / "workloads" / "mnist"
BASE_URL = "https://storage.googleapis.com/cvdf-datasets/mnist"
FILES = (
    "train-images-idx3-ubyte.gz",
    "train-labels-idx1-ubyte.gz",
    "t10k-images-idx3-ubyte.gz",
    "t10k-labels-idx1-ubyte.gz",
)
FILTERS = 8
CLASS_COUNT = 10


def fetch_mnist():
    RAW_DIR.mkdir(parents=True, exist_ok=True)
    for filename in FILES:
        destination = RAW_DIR / filename
        if destination.exists():
            continue
        print(f"Downloading MNIST {filename}")
        request = urllib.request.Request(
            f"{BASE_URL}/{filename}",
            headers={"User-Agent": "CNNsimulator-MNIST-workload/1.0"},
        )
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                destination.write_bytes(response.read())
        except Exception:
            destination.unlink(missing_ok=True)
            raise


def read_images(filename):
    with gzip.open(filename, "rb") as stream:
        magic, count, height, width = struct.unpack(">IIII", stream.read(16))
        if magic != 2051 or (height, width) != (28, 28):
            raise ValueError(f"Invalid MNIST image file: {filename}")
        raw = stream.read()
    expected = count * height * width
    if len(raw) != expected:
        raise ValueError(f"MNIST image file has invalid length: {filename}")
    return np.frombuffer(raw, dtype=np.uint8).reshape(count, height, width)


def read_labels(filename):
    with gzip.open(filename, "rb") as stream:
        magic, count = struct.unpack(">II", stream.read(8))
        if magic != 2049:
            raise ValueError(f"Invalid MNIST label file: {filename}")
        raw = stream.read()
    if len(raw) != count or np.any(np.frombuffer(raw, dtype=np.uint8) > 9):
        raise ValueError(f"MNIST label file has invalid content: {filename}")
    return np.frombuffer(raw, dtype=np.uint8).copy()


def downsample(images):
    # Non-overlapping 4x4 averages retain the coarse digit shape in 7x7.
    small = images.reshape(-1, 7, 4, 7, 4).mean(axis=(2, 4))
    return (small / 255.0).astype(np.float32)


def patches(images):
    return np.lib.stride_tricks.sliding_window_view(
        images, (3, 3), axis=(1, 2)
    )


def forward(images, conv_weights, conv_biases, dense_weights, dense_biases):
    padded = np.pad(images[..., None], ((0, 0), (1, 1), (1, 1), (0, 0)))
    windows = patches(padded)
    convolution = np.einsum(
        "nhwcij,fcij->nfhw", windows, conv_weights, optimize=True
    )
    convolution += conv_biases[None, :, None, None]
    convolution = np.maximum(convolution, 0)

    pool_windows = (
        convolution[:, :, :6, :6].reshape(-1, FILTERS, 3, 2, 3, 2)
        .transpose(0, 1, 2, 4, 3, 5)
        .reshape(-1, FILTERS, 3, 3, 4)
    )
    pool_indices = pool_windows.argmax(axis=-1)
    pooled = pool_windows.max(axis=-1)

    features = pooled.reshape(len(images), -1)
    logits = features @ dense_weights + dense_biases
    return logits, (windows, convolution, pool_indices, pooled, features)


def backward(labels, logits, cache, conv_weights, dense_weights):
    windows, convolution, pool_indices, pooled, features = cache
    shifted = logits - logits.max(axis=1, keepdims=True)
    probabilities = np.exp(shifted)
    probabilities /= probabilities.sum(axis=1, keepdims=True)
    probabilities[np.arange(len(labels)), labels] -= 1.0
    probabilities /= len(labels)

    dense_grad = features.T @ probabilities
    dense_bias_grad = probabilities.sum(axis=0)
    feature_grad = probabilities @ dense_weights.T
    pool_grad = feature_grad.reshape(pooled.shape)

    pool_blocks = np.zeros((*pool_grad.shape, 4), dtype=np.float32)
    np.put_along_axis(
        pool_blocks, pool_indices[..., None], pool_grad[..., None], axis=-1
    )
    convolution_grad = (
        pool_blocks.reshape(-1, FILTERS, 3, 3, 2, 2)
        .transpose(0, 1, 2, 4, 3, 5)
        .reshape(-1, FILTERS, 6, 6)
    )
    convolution_grad = np.pad(
        convolution_grad, ((0, 0), (0, 0), (0, 1), (0, 1))
    )
    convolution_grad *= convolution > 0

    conv_weight_grad = np.einsum(
        "nfhw,nhwcij->fcij", convolution_grad, windows, optimize=True
    )
    conv_bias_grad = convolution_grad.sum(axis=(0, 2, 3))
    return (
        conv_weight_grad,
        conv_bias_grad,
        dense_grad,
        dense_bias_grad,
    )


def evaluate(images, labels, conv_weights, conv_biases, dense_weights, dense_biases):
    correct = 0
    for start in range(0, len(labels), 512):
        logits, _ = forward(
            images[start : start + 512],
            conv_weights,
            conv_biases,
            dense_weights,
            dense_biases,
        )
        correct += int(
            (logits.argmax(axis=1) == labels[start : start + 512]).sum()
        )
    return correct / len(labels)


def train(images, labels, validation_images, validation_labels, epochs, batch_size, seed):
    rng = np.random.default_rng(seed)
    conv_weights = rng.normal(
        0, math.sqrt(2 / 9), size=(FILTERS, 1, 3, 3)
    ).astype(np.float32)
    conv_biases = np.zeros(FILTERS, dtype=np.float32)
    dense_weights = rng.normal(
        0, math.sqrt(2 / (FILTERS * 9)), size=(FILTERS * 9, CLASS_COUNT)
    ).astype(np.float32)
    dense_biases = np.zeros(CLASS_COUNT, dtype=np.float32)
    parameters = [conv_weights, conv_biases, dense_weights, dense_biases]
    first_moments = [np.zeros_like(value) for value in parameters]
    second_moments = [np.zeros_like(value) for value in parameters]
    step = 0
    learning_rate = 0.003

    for epoch in range(epochs):
        order = rng.permutation(len(labels))
        total_loss = 0.0
        correct = 0
        for start in range(0, len(order), batch_size):
            indices = order[start : start + batch_size]
            batch_images = images[indices]
            batch_labels = labels[indices]
            logits, cache = forward(
                batch_images,
                conv_weights,
                conv_biases,
                dense_weights,
                dense_biases,
            )
            shifted = logits - logits.max(axis=1, keepdims=True)
            log_normalizer = np.log(np.exp(shifted).sum(axis=1))
            total_loss += float(
                (-shifted[np.arange(len(batch_labels)), batch_labels] +
                 log_normalizer).sum()
            )
            correct += int((logits.argmax(axis=1) == batch_labels).sum())
            gradients = backward(
                batch_labels, logits, cache, conv_weights, dense_weights
            )[:4]
            step += 1
            beta1, beta2 = 0.9, 0.999
            for index, (parameter, gradient) in enumerate(
                zip(parameters, gradients)
            ):
                first_moments[index] *= beta1
                first_moments[index] += (1 - beta1) * gradient
                second_moments[index] *= beta2
                second_moments[index] += (1 - beta2) * gradient * gradient
                first = first_moments[index] / (1 - beta1**step)
                second = second_moments[index] / (1 - beta2**step)
                parameter -= learning_rate * first / (np.sqrt(second) + 1e-8)

        count = len(labels)
        validation_accuracy = evaluate(
            validation_images,
            validation_labels,
            conv_weights,
            conv_biases,
            dense_weights,
            dense_biases,
        )
        print(
            f"epoch {epoch + 1}/{epochs}: "
            f"loss={total_loss / count:.4f}, "
            f"train_accuracy={correct / count:.2%}, "
            f"validation_accuracy={validation_accuracy:.2%}"
        )
    return conv_weights, conv_biases, dense_weights, dense_biases


def quantize_weights(weights):
    maximum = float(np.max(np.abs(weights)))
    scale = maximum / 127.0 if maximum else 1.0
    quantized = np.clip(np.rint(weights / scale), -127, 127).astype(np.int32)
    return quantized, scale


def shift_for_range(maximum):
    if maximum <= 120:
        return 0
    return int(math.ceil(math.log2(maximum / 120.0)))


def write_values(path, values, values_per_line=49):
    flat = np.asarray(values).reshape(-1)
    with path.open("w", encoding="ascii") as stream:
        for start in range(0, len(flat), values_per_line):
            stream.write(
                " ".join(str(int(value)) for value in flat[start : start + values_per_line])
                + "\n"
            )


def export(
    test_images,
    test_labels,
    conv_weights,
    conv_biases,
    dense_weights,
    dense_biases,
):
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    qconv, conv_weight_scale = quantize_weights(conv_weights)
    qdense, dense_weight_scale = quantize_weights(dense_weights.T)

    conv_input_scale = 1 / 127
    conv_accumulator_scale = conv_input_scale * conv_weight_scale
    conv_bias_int = np.rint(conv_biases / conv_accumulator_scale)
    if np.any(np.abs(conv_bias_int) > np.iinfo(np.int32).max):
        raise OverflowError("Quantized convolution bias exceeds int32")
    conv_shift = shift_for_range(
        float(np.max(np.maximum(conv_biases, 0)) / conv_accumulator_scale)
    )

    # Calibrate quantization from a fixed prefix of training inputs, not test.
    calibration = downsample(read_images(RAW_DIR / FILES[0])[:2048])
    calibration_windows = patches(
        np.pad(calibration[..., None], ((0, 0), (1, 1), (1, 1), (0, 0)))
    )
    calibration_conv = np.einsum(
        "nhwcij,fcij->nfhw",
        calibration_windows,
        conv_weights,
        optimize=True,
    )
    calibration_conv += conv_biases[None, :, None, None]
    max_conv_accumulator = float(
        np.maximum(calibration_conv, 0).max() / conv_accumulator_scale
    )
    conv_shift = shift_for_range(max_conv_accumulator)

    conv_output_scale = conv_accumulator_scale * (2**conv_shift)
    dense_accumulator_scale = conv_output_scale * dense_weight_scale
    dense_bias_int = np.rint(dense_biases / dense_accumulator_scale)
    if np.any(np.abs(dense_bias_int) > np.iinfo(np.int32).max):
        raise OverflowError("Quantized fully-connected bias exceeds int32")

    dense_calibration = (
        forward(
            calibration,
            conv_weights,
            conv_biases,
            dense_weights,
            dense_biases,
        )[0]
    )
    dense_shift = shift_for_range(
        float(
            np.max(np.abs(dense_calibration)) / dense_accumulator_scale
        )
    )

    calibration_quantized = np.rint(calibration * 127).astype(np.int32)
    calibration_windows_int = patches(
        np.pad(
            calibration_quantized[..., None],
            ((0, 0), (1, 1), (1, 1), (0, 0)),
        )
    )
    calibration_conv_int = np.einsum(
        "nhwcij,fcij->nfhw",
        calibration_windows_int,
        qconv,
        optimize=True,
    )
    calibration_conv_int += conv_bias_int.astype(np.int64)[None, :, None, None]
    calibration_conv_int = np.clip(
        np.right_shift(calibration_conv_int, conv_shift), 0, 127
    )
    calibration_pooled = (
        calibration_conv_int[:, :, :6, :6]
        .reshape(-1, FILTERS, 3, 2, 3, 2)
        .max(axis=(3, 5))
        .reshape(len(calibration), -1)
    )
    calibration_logits = (
        calibration_pooled @ qdense.T + dense_bias_int.astype(np.int64)
    )
    dense_shift = shift_for_range(float(np.max(np.abs(calibration_logits))))

    test_small = downsample(test_images)
    test_quantized = np.rint(test_small * 127).astype(np.int32)
    conv_windows = patches(
        np.pad(
            test_quantized[..., None],
            ((0, 0), (1, 1), (1, 1), (0, 0)),
        )
    )
    integer_conv = np.einsum(
        "nhwcij,fcij->nfhw",
        conv_windows,
        qconv,
        optimize=True,
    )
    integer_conv += conv_bias_int.astype(np.int64)[None, :, None, None]
    integer_conv = np.clip(np.right_shift(integer_conv, conv_shift), 0, 127)
    pooled = (
        integer_conv[:, :, :6, :6]
        .reshape(-1, FILTERS, 3, 2, 3, 2)
        .max(axis=(3, 5))
        .reshape(len(test_images), -1)
    )
    integer_logits = pooled @ qdense.T + dense_bias_int.astype(np.int64)
    integer_logits = np.clip(
        np.right_shift(integer_logits, dense_shift), -128, 127
    )
    predictions = integer_logits.argmax(axis=1)
    accuracy = float(np.mean(predictions == test_labels))

    write_values(OUTPUT_DIR / "conv_weights.dat", qconv)
    write_values(OUTPUT_DIR / "conv_biases.dat", conv_bias_int)
    write_values(OUTPUT_DIR / "fc_weights.dat", qdense)
    write_values(OUTPUT_DIR / "fc_biases.dat", dense_bias_int)
    write_values(
        OUTPUT_DIR / "test_images.dat",
        np.rint(test_small * 127).astype(np.int32),
    )
    write_values(OUTPUT_DIR / "test_labels.dat", test_labels)

    model = OUTPUT_DIR / "model.dat"
    model.write_text(
        "\n".join(
            (
                "# Generated by tools/train_mnist.py",
                "CNN_MODEL 1",
                "INPUT 1 7 7",
                f"SAMPLES {len(test_labels)}",
                "LAYERS 4",
                f"CONV2D {FILTERS} 3 3 1 1 {conv_shift} 1 "
                "conv_weights.dat conv_biases.dat",
                "MAXPOOL2D 2 2 2",
                "FLATTEN",
                f"FC {CLASS_COUNT} {dense_shift} 0 "
                "fc_weights.dat fc_biases.dat",
                "",
            )
        ),
        encoding="ascii",
    )
    print(
        f"Exported {len(test_labels)} MNIST test samples to {OUTPUT_DIR}; "
        f"quantized reference accuracy={accuracy:.2%}"
    )
    print(f"quantization shifts: conv={conv_shift}, fc={dense_shift}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--epochs", type=int, default=5)
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument(
        "--train-samples",
        type=int,
        default=0,
        help="Use a prefix of training data; 0 uses all 60,000 images.",
    )
    args = parser.parse_args()
    if args.epochs <= 0 or args.batch_size <= 0 or args.train_samples < 0:
        parser.error("epochs and batch-size must be positive; train-samples nonnegative")

    fetch_mnist()
    train_images = read_images(RAW_DIR / FILES[0])
    train_labels = read_labels(RAW_DIR / FILES[1]).astype(np.int32)
    test_images = read_images(RAW_DIR / FILES[2])
    test_labels = read_labels(RAW_DIR / FILES[3]).astype(np.int32)
    if args.train_samples:
        train_images = train_images[: args.train_samples]
        train_labels = train_labels[: args.train_samples]

    split_rng = np.random.default_rng(args.seed)
    order = split_rng.permutation(len(train_labels))
    validation_count = max(1, len(train_labels) // 10)
    validation_indices = order[:validation_count]
    training_indices = order[validation_count:]
    x_train = downsample(train_images[training_indices])
    y_train = train_labels[training_indices]
    x_validation = downsample(train_images[validation_indices])
    y_validation = train_labels[validation_indices]
    print(
        f"MNIST train: {len(y_train)}, validation: {len(y_validation)}, "
        f"test: {len(test_labels)}; source images 28x28 grayscale, 10 classes"
    )
    model = train(
        x_train,
        y_train,
        x_validation,
        y_validation,
        args.epochs,
        args.batch_size,
        args.seed,
    )
    export(test_images, test_labels, *model)


if __name__ == "__main__":
    main()
