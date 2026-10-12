#include "PE.h"

#include "MAC.h"
#include "MUX.h"
#include "Register.h"

#include <memory>

namespace {
    using WeightType = PE::WeightType;
    using IfmapType  = PE::IfmapType;
    using PsumType   = PE::PsumType;

    using MACType = MAC<WeightType, IfmapType, PsumType>;
    using MuxType = MUX<2, PsumType>;
    using WeightRegister = Register<8>;
    using IfmapRegister  = Register<8>;
}

struct PE::Impl {
    explicit Impl(PEArchitecture arch): architecture(arch) {}

    PEArchitecture architecture;

    // Internal signals
    sc_core::sc_signal<WeightType> weight_reg_signal{"weight_reg_signal"};
    sc_core::sc_signal<IfmapType> ifmap_reg_signal{"ifmap_reg_signal"};
    sc_core::sc_signal<PsumType> mac_result{"mac_result"};
    sc_core::sc_signal<PsumType> mux_psum{"mux_psum"};
    sc_core::sc_signal<MuxType::SelectType> mux_select{"mux_select"};
    sc_core::sc_signal<bool> mux_select_valid{"mux_select_valid"};

    // Hardware blocks
    std::unique_ptr<MACType> mac;
    std::unique_ptr<MuxType> mux;

    std::unique_ptr<WeightRegister> weight_register;
    std::unique_ptr<IfmapRegister> ifmap_register;
};

PE::PE(
    sc_core::sc_module_name name,
    PEArchitecture architecture
)
    : sc_core::sc_module(name),
      impl_(std::make_unique<Impl>(architecture))
{
    // MAC is common to all architectures.
    impl_->mac = std::make_unique<MACType>("mac");

    impl_->mac->clk(clk);
    impl_->mac->reset(reset);
    impl_->mac->enable(enable);
    impl_->mac->result(impl_->mac_result);

    // --------------------
    // Output Stationary
    // --------------------
    if (architecture == PEArchitecture::OS) {
        impl_->mux = std::make_unique<MuxType>("psum_mux");

        impl_->mux->input[0](psum_in);
        impl_->mux->input[1](impl_->mac_result);
        impl_->mux->select(impl_->mux_select);
        impl_->mux->output(impl_->mux_psum);
        impl_->mux->select_valid(impl_->mux_select_valid);

        impl_->mac->weight(weight_in);
        impl_->mac->ifmap(ifmap_in);
        impl_->mac->psum(impl_->mux_psum);
    }

    // --------------------
    // Weight Stationary
    // --------------------
    else if (architecture == PEArchitecture::WS) {
        impl_->weight_register = std::make_unique<WeightRegister>("weight_register");

        impl_->weight_register->clk(clk);
        impl_->weight_register->reset(reset);
        impl_->weight_register->enable(load_weight);
        impl_->weight_register->data_in(weight_in);
        impl_->weight_register->data_out(impl_->weight_reg_signal);

        impl_->mac->weight(impl_->weight_reg_signal);
        impl_->mac->ifmap(ifmap_in);
        impl_->mac->psum(psum_in);
    }

    // --------------------
    // Input Stationary
    // --------------------
    else if (architecture == PEArchitecture::IS) {
        impl_->ifmap_register = std::make_unique<IfmapRegister>("ifmap_register");

        impl_->ifmap_register->clk(clk);
        impl_->ifmap_register->reset(reset);
        impl_->ifmap_register->enable(load_ifmap);
        impl_->ifmap_register->data_in(ifmap_in);
        impl_->ifmap_register->data_out(impl_->ifmap_reg_signal);

        impl_->mac->weight(weight_in);
        impl_->mac->ifmap(impl_->ifmap_reg_signal);
        impl_->mac->psum(psum_in);
    }

    // --------------------
    // No local stationary state
    // --------------------
    else {
        impl_->mac->weight(weight_in);
        impl_->mac->ifmap(ifmap_in);
        impl_->mac->psum(psum_in);
    }

    SC_METHOD(select_psum_source);
    sensitive << use_feedback;

    SC_METHOD(drive_output);
    sensitive << impl_->mac_result;
}

PE::~PE() = default;

void PE::select_psum_source()
{
    if (!impl_->mux) {
        return;
    }

    // 0: external partial sum
    // 1: feedback from the previous MAC result
    impl_->mux_select.write(
        MuxType::SelectType(use_feedback.read() ? 1 : 0)
    );
}

void PE::drive_output()
{
    psum_out.write(impl_->mac_result.read());
}