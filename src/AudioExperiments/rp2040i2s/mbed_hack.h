#pragma once

static inline bool dma_irqn_get_channel_status(uint irq_index, uint channel) {
    invalid_params_if(DMA, irq_index > 1);
    check_dma_channel_param(channel);
    return (irq_index ? dma_hw->ints1 : dma_hw->ints0) & (1u << channel);
}
static inline void dma_irqn_set_channel_enabled(uint irq_index, uint channel, bool enabled) {
    invalid_params_if(DMA, irq_index > 1);
    if (irq_index) {
        dma_channel_set_irq1_enabled(channel, enabled);
    } else {
        dma_channel_set_irq0_enabled(channel, enabled);
    }
}
static inline void dma_irqn_acknowledge_channel(uint irq_index, uint channel) {
    invalid_params_if(DMA, irq_index > 1);
    check_dma_channel_param(channel);
    hw_set_bits(irq_index ? &dma_hw->ints1 : &dma_hw->ints0, (1u << channel));
}

