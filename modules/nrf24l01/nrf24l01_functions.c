#include <linux/gpio.h>
#include <linux/regmap.h>

#include "nrf24l01_core.h"
#include "nrf24l01_reg.h"
#include "nrf24l01_spi.h"
#include "partregmap.h"

int nrf24l01_set_channel(struct nrf24l01_t* nrf, unsigned int ch)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RF_CH_RF_CH, &ch, 1);
}

int nrf24l01_get_channel(struct nrf24l01_t* nrf, unsigned int* ch)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_RF_CH_RF_CH, ch, 1);
}

int nrf24l01_set_tx_power(struct nrf24l01_t* nrf, int tx_pwr)
{
	unsigned int pwr;
	switch(tx_pwr)
	{
		case 0:
			pwr = 0b11;
			break;
		case -6:
			pwr = 0b10;
			break;
		case -12:
			pwr = 0b01;
			break;
		case -18:
			pwr = 0b00;
			break;
		default:
			return -EINVAL;
	}
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RF_SETUP_RF_PWR, &pwr, 1);
}

int nrf24l01_get_tx_power(struct nrf24l01_t* nrf, int* tx_pwr)
{
	unsigned int pwr;
	int err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_RF_SETUP_RF_PWR, &pwr, 1);
	if(err < 0)
		return err;
	if(pwr > 0b11)
		return -EINVAL;
	return -18 + pwr * 6;
}

int nrf24l01_set_address_width(struct nrf24l01_t* nrf, unsigned int width)
{
	if(width < 3 || width > 5)
		return -EINVAL;
	width -= 2;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_SETUP_AW_AW, &width, 1);
}

int nrf24l01_get_address_width(struct nrf24l01_t* nrf, unsigned int* width)
{
	int err = partreg_table_read(nrf->reg_table, NRF24L01_VREG_SETUP_AW_AW, width, 1);
	if(err < 0)
		return err;
	if(*width < 1 || *width > 3)
		return -EINVAL; 
	*width += 2;
	return 0;
}

int nrf24l01_flush_rx(struct nrf24l01_t* nrf)
{
	int err;
	mutex_lock(&nrf->m_rx_path);
	err = nrf24l01_spi_flush_rx(nrf);
	mutex_unlock(&nrf->m_rx_path);
	return err;
}

int nrf24l01_flush_tx(struct nrf24l01_t* nrf)
{
	int err;
	mutex_lock(&nrf->m_tx_path);
	err = nrf24l01_spi_flush_tx(nrf);
	mutex_unlock(&nrf->m_tx_path);
	return err;
}

int nrf24l01_flush(struct nrf24l01_t* nrf)
{
	int err = nrf24l01_flush_rx(nrf);
	if(err < 0)
		return err;
	return nrf24l01_flush_tx(nrf);
}

int nrf24l01_set_pwr_up(struct nrf24l01_t* nrf, unsigned int state)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_PWR_UP, &state, 1);
}

int nrf24l01_pwr_up(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_pwr_up(nrf, 1);
}

int nrf24l01_pwr_down(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_pwr_up(nrf, 0);
}

int nrf24l01_get_pwr_state(struct nrf24l01_t* nrf, unsigned int* state)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_CONFIG_PWR_UP, state, 1);
}

int nrf24l01_set_prim_rx(struct nrf24l01_t* nrf, unsigned int state)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_CONFIG_PRIM_RX, &state, 1);
}

int nrf24l01_set_rx(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_prim_rx(nrf, 1);
}

int nrf24l01_set_tx(struct nrf24l01_t* nrf)
{
	return nrf24l01_set_prim_rx(nrf, 0);
}

int nrf24l01_get_prim_rx(struct nrf24l01_t* nrf, unsigned int* state)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_CONFIG_PRIM_RX, state, 1);
}

int nrf24l01_set_pld_width(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int width)
{
	if(pipe > 5)
		return -EINVAL;
	if(width > 32)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RX_PW_P0 + pipe, &width, 1);
}

int nrf24l01_get_pld_width(struct nrf24l01_t* nrf, unsigned int pipe, unsigned int* width)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_RX_PW_P0 + pipe, width, 1);	
}

int nrf24l01_set_rx_addr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned char* addr, unsigned int len)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*)addr, len);
}

int nrf24l01_get_rx_addr(struct nrf24l01_t* nrf, unsigned int pipe, unsigned char* addr, unsigned int len)
{
	if(pipe > 5)
		return -EINVAL;
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_RX_ADDR_P0 + pipe, (unsigned int*)addr, len);
}

int nrf24l01_set_tx_addr(struct nrf24l01_t* nrf, unsigned char* addr, unsigned int len)
{
	return partreg_table_write(nrf->reg_table, NRF24L01_VREG_TX_ADDR, (unsigned int*)addr, len);
}

int nrf24l01_get_tx_addr(struct nrf24l01_t* nrf, unsigned char* addr, unsigned int len)
{
	return partreg_table_read(nrf->reg_table, NRF24L01_VREG_TX_ADDR, (unsigned int*)addr, len);
}

void nrf24l01_set_ce(struct nrf24l01_t* nrf, unsigned int state)
{
	gpio_set_value(nrf->gpio_ce, state);
}

int nrf24l01_send_packet(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len)
{

}
