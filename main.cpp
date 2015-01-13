#include "stm32f0xx.h"

constexpr uint64_t F_CPU = 54000000ULL;

#include "pinlist.h"
#include "utils.h"
#include "delay.h"
//#include "clock.h"
#include "timers.h"
//#include <cstdlib>
#include "dma.h"
#include "adc.h"
#include "streams.h"

#define USARTECHO
#include "usart.h"

using namespace Mcucpp;
using Usart = Usarts::UsartIrq<USART1_BASE, Usarts::RemapUsart1_Pa9Pa10, 128, 16>;
DECLAREIRQ(Usart, USART1)

void ClockInit()
{
	__IO uint32_t StartUpCounter = 0, HSEStatus = 0;

	/* SYSCLK, HCLK, PCLK configuration ----------------------------------------*/
	/* Enable HSE */
	RCC->CR |= ((uint32_t)RCC_CR_HSEON);

	/* Wait till HSE is ready and if Time out is reached exit */
	do
	{
		HSEStatus = RCC->CR & RCC_CR_HSERDY;
		StartUpCounter++;
	} while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

	if((RCC->CR & RCC_CR_HSERDY) != RESET)
	{
		HSEStatus = (uint32_t)0x01;
	}
	else
	{
		HSEStatus = (uint32_t)0x00;
	}

	if (HSEStatus == (uint32_t)0x01)
	{
		/* Enable Prefetch Buffer and set Flash Latency */
		FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;

		/* HCLK = SYSCLK */
		RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;

		/* PCLK = HCLK */
		RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE_DIV1;

		/* PLL configuration = HSE * 2 = 54 MHz */
		RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
		RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_PREDIV1 | RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLMULL2);

		/* Enable PLL */
		RCC->CR |= RCC_CR_PLLON;

		/* Wait till PLL is ready */
		while((RCC->CR & RCC_CR_PLLRDY) == 0)
		{
		}

		/* Select PLL as system clock source */
		RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
		RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;

		/* Wait till PLL is used as system clock source */
		while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)RCC_CFGR_SWS_PLL)
		{
		}
	}
	else
	{ /* If HSE fails to start-up, the application will have wrong clock
		 configuration. User can add here some code to deal with this error */
	}
}
constexpr uint8_t channelsQuantity = 4;
static uint8_t channels = 2;

constexpr uint16_t GetDivider(uint8_t nf)
{
	return nf == 8 ? 2 : 1 << ((8 - nf) << 1);
}

static uint16_t adcSamplesArr[2][8][channelsQuantity];		//buf x nsample x nchannel

extern "C"
{
	void HardFault_Handler()
	{
		while(true)
		{
			delay_ms<50>();
		//GreenLed::Toggle();
		}
	}
	void DMA1_Channel1_IRQHandler()
	{
		static uint16_t adcSamples[8];
		uint8_t nbuf;
		if(DMA1->ISR & DMA_ISR_HTIF1)	//half transfer
			nbuf = 0;
		else if(DMA1->ISR & DMA_ISR_TCIF1)	//transfer complete
			nbuf = 1;
		else goto END;
		for(uint8_t c = 0; c < channels; ++c)
		{
			for(uint8_t s = 0; s < 8; ++s)
			{
				adcSamples[c] += (*(uint16_t(*)[2][8][channels])adcSamplesArr)[nbuf][s][c];
			}
				Usart::NewLine();
				Usart::Puts<DataFormat::Dec>(adcSamples[c] / 8);
				adcSamples[c] = 0;
				if(c < channels - 1) Usart::Putch(';');
		}
		Usart::Putch('\r');
	END:
		DMA1->IFCR = DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CGIF1;
		__NOP();
	}
}

int main()
{
	ClockInit();
	Dmas::EnableClock();
	{
		using namespace Gpio;
		EnablePorts<Porta, Portb>();
	}

	Usart::Init<Usarts::DefaultCfg, Usarts::BaudRate<921600UL>>();
	{
		using namespace Timers;
		Tim3::Init<ARRbuffered | UpCount, F_CPU / (16384 * 8), 2>();
		Tim3::MasterModeSelect(MM_Update);
		Tim3::Enable();
	}{
		using namespace Adcs;
		Adc::Init<Single | DownScan,
				Clock::PclkDiv4,
				InitChannels<Ch::Pa0_0 | Ch::Pa1_1 | Ch::Pa2_2 | Ch::Pa3_3
								| Ch::Pa4_4 | Ch::Pa5_5 | Ch::Pa6_6 | Ch::Pa7_7>,
				EnableExtTrigger<TrigEdge::Rising, TrigEvent::Tim3_Trgo>,
				EnableDma<DmaMode::Circular>
										>();
		Adc::SetTsample<Tsample::_41c5>();
	}{
		using namespace Dmas;
		Adc::DmaInit<Circular | HalfTransferIRQ | TransferCompleteIRQ>(adcSamplesArr);
	}

	uint32_t channelsMask = Populate(channels) << (channelsQuantity - channels);
	Adc::SelectChannels(channelsMask);
	Adc::Dma::SetCounter(channels * 16);		//8 samples, double buffering
	Adc::Dma::Enable();
	Adc::Start();
	Usart::Puts("\r\nAnalog Data Logger v1.0\r\n");
	//TODO: Enable watchdog
	while(true)
	{
		delay_us<40>();
	}
	uint8_t* str;
	while(true)
	{
		while(true)
		{
			Usart::Puts("\r\nEnter Min voltage in mV (in range 500...4200mV): ");
			while(!(str = Usarts::GetStr<Usart>()))
				;
		}
	}
}

