#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/semaphore.h>

#include "nrf24l01_core.h"
#include "nrf24l01_worker.h"
#include "nrf24l01_functions.h"

int nrf24l01_create_worker(struct nrf24l01_t* nrf)
{
	sema_init(&nrf->worker.sema, 0);
	nrf->worker.thread = kthread_run(nrf24l01_worker_do_work, nrf, "nrf_worker");
	if(IS_ERR(nrf->worker.thread))
		return PTR_ERR(nrf->worker.thread);
	return 0;
}

int nrf24l01_destroy_worker(struct nrf24l01_t* nrf)
{
	return kthread_stop(nrf->worker.thread);
}

static int nrf24l01_worker_do_work(void* ctx)
{
	int err;
	unsigned int data;
	struct nrf24l01_t* nrf = (nrf24l01_t*) ctx;
	while(!kthread_should_stop())
	{
		if((err = down_timeout(&nrf->worker.sema, HZ)))
		{
			if(err != -ETIME)
			{
				dev_err(&nrf->spi->dev, "Worker thread failed! err=%d\n", err);
//				do_exit(err);
			}
			if(kthread_should_stop())
				break;
			continue;
		}
		printk(KERN_INFO "Event!\n");
		mutex_lock(&nrf->m_rx_path);
		if((err = nrf24l01_get_status_rx_dr(nrf, &data)))
		{
			dev_err(&nrf->spi->dev, "Failed to get rx_dr flag\n");
		}
		if(data)
		{
			up(&nrf->rx);
		}
		mutex_unlock(&nrf->m_rx_path);
		mutex_lock(&nrf->m_tx_path);
		if((err = nrf24l01_get_status_tx_ds(nrf, &data)))
		{
			dev_err(&nrf->spi->dev, "Failed to get tx_ds flag\n");
		}
		if(data)
		{
			up(&nrf->tx);
		}
		if((err = nrf24l01_get_status_max_rt(nrf, &data)))
		{
			dev_err(&nrf->spi->dev, "Failed to get max_rt flag\n");
		}
		if(data)
		{
			dev_warn(&nrf->spi->dev, "Maximum number of retransmissions reached. Dropping packet\n");
			data = 1;
			if((err = nrf24l01_set_status_max_rt(nrf, data)))
			{
				dev_err(&nrf->spi->dev, "Failed to reset max_rt flag");
			}
			up(&nrf->tx);
		}
		mutex_unlock(&nrf->m_tx_path);
//		usleep_range(500000, 1000000);
	}
	return 0;
}