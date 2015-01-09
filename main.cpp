#include "stm32f0xx.h"

#include "pinlist.h"
#include "utils.h"
#include "delay.h"
//#include "clock.h"
#include "timers.h"
//#include <cstdlib>
#include "adc.h"
#include "streams.h"

using GreenLed = Mcucpp::Pb8;
using BlueLed = Mcucpp::Pb7;

#define USARTECHO
#include "usart.h"

using namespace Mcucpp;
using Usart = Usarts::UsartIrq<USART1_BASE, Usarts::DefaultRemap<USART1_BASE>, 128, 16>;
DECLAREIRQ(Usart, USART1)

extern "C"
{
	void HardFault_Handler()
	{
		while(true)
		{
			delay_ms<50>();
			GreenLed::Toggle();
		}
	}
	void SysTick_Handler()
	{
	}
}

void Putc(uint8_t ch)
{
	Usart::Putch(ch);
}
int main()
{
	{
		using namespace Gpio;
		EnablePorts<Porta, Portb>();
		GreenLed::SetConfig<OutputSlow, PushPull>();
		Pb0::SetConfig<Input, Analog>();
		Pa8::SetConfig<OutputFastest, AltPushPull>();
		Pa8::AltFuncNumber<AF::_2>();					//TIM1 CH1
		Pa6::SetConfig<OutputFast, PushPull>();
		Pa6::Clear();
	}
	Usart::Init<Usarts::DefaultCfg, Usarts::BaudRate<921600UL>>();

	{
		using namespace Timers;
		Tim1::Init<ARRbuffered | UpCount, 1, 512>();
		Tim1::ChannelInit<Ch1, Output, Out_PwmMode1>();
		Tim1::ChannelEnable<Ch1>();
		Tim1::Enable();
	}

	Adc::Init<Adcs::DefaultConfig | Adcs::Discontinuous,
			Adcs::Clock::PclkDiv4,
			Adcs::InitChannels<Adcs::Ch::Pb0_8 | Adcs::Ch::Pb1_9, true>
									>();
	Adc::SetTsample<Adcs::Tsample::_71c5>();

	IO::Ostream cout(Putc);
	__DSB();
	cout << 20UL/* << IO::endl*/;
	__DSB();
//	Usart::Puts("\r\nHello");
//-------
	uint8_t* str;
	while(true)
	{
		while(true)
		{
			Usart::Puts("\r\nEnter Min voltage in mV (in range 500...4200mV): ");
			while(!(str = Usarts::GetStr<Usart>()))
				;
		}
		GreenLed::Clear();
		Tim1::Ccr1() = 4;
		SysTick_Config(100 * (F_CPU/(1000 * 8)), true);
	}
}

