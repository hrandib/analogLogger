#include "stm32f0xx.h"

#include "pinlist.h"
#include "utils.h"
#include "delay.h"
//#include "hd44780.h"
#include "nokia1xxx_lcd.h"
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

//using Spi = Spis::SpiIrq<SPI1_BASE, Spis::SendOnly, Spis::Framewidth<Spis::Bits::_9>>;
//DECLAREIRQ(Spi, SPI1)

//using Lcd = Hd44780<Gpio::Pinlist<Pa4, Gpio::SequenceOf<4>>, Pa0, Pa1>;
using Lcd = Nokia::Lcd<>;
struct VI
{
	uint16_t v, i;
};

static volatile uint16_t Uthreshold{2900}, Itarget{400};
static volatile bool isFinished;
static VI GetSampleMean16()
{
	VI values = {0, 0};
	for(uint8_t nsamples = 0; nsamples < 16; ++nsamples)
	{
		values.v += Adc::GetSample();
		values.i += Adc::GetSample();
	}
	values.v = values.v/16 + ((uint32_t)values.v * 2)/405;
	values.i /= 60;
	if(values.i) values.i += 3;
	return values;
}
static void DisplayElaplsedTime(uint32_t seconds)
{
	uint8_t hours{0}, minutes{0};
	while(seconds >= 3600)
	{
		++hours;
		seconds -= 3600;
	}
	while(seconds >= 60)
	{
		++minutes;
		seconds -= 60;
	}
	Lcd::SetXY(0, 0);
	Lcd::Fill(0, 0, 96, Nokia::Clear);
	uint8_t buf[4];
	Lcd::SetXY(5, 0);
	Lcd::Puts(utoa(hours, buf));
	Lcd::Putch(':');
	Lcd::Puts(utoa(minutes, buf));
	Lcd::Putch(':');
	Lcd::Puts(utoa(seconds, buf));
}

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
		static uint32_t counter, mAsecs, mVsecs;
		static uint16_t ItargetMin;
		VI values = GetSampleMean16();
	// Current regulation
		if(values.i < Itarget && values.i > 5) ++Tim1::Ccr1();
		else if(values.i > Itarget) --Tim1::Ccr1();

		if(values.v <= Uthreshold + 50)		//Smooth decrease current at the end of discharge
		{
			if(!ItargetMin) ItargetMin = Itarget / 3;
			if(Itarget >= ItargetMin) --Itarget;
		}
		if(values.v < Uthreshold)			//End condition
		{
			Tim1::Ccr1() = 0;
			SysTick_DisableInterrupt();
			GreenLed::Set();
			counter = mAsecs = mVsecs = ItargetMin = 0;
			isFinished = true;
		}
		if(counter && !(counter % 601))				//Measure ESR
		{
			static bool MeasureESR;
			static uint16_t PWMvalue;
			static VI underLoad;
			if(!MeasureESR)
			{
				PWMvalue = Tim1::Ccr1();
				Tim1::Ccr1() = 0;
				underLoad = values;
				MeasureESR = true;
			}
			else
			{
				++counter;
				Tim1::Ccr1() = PWMvalue;
				uint16_t Esr = ((values.v - underLoad.v) * 1000)/underLoad.i - 20;
				Lcd::Fill(0, 6, 96, Nokia::Clear);
				uint8_t buf[6];
				Lcd::Puts("ESR: ");
				Lcd::Puts(utoa(Esr, buf));
				Lcd::Puts(" mOhm");
				MeasureESR = false;
			}
		}
		else if(!(++counter % 10))
		{
			uint32_t secs = (counter - counter / 601) / 10;
			uint8_t buf[8];
			mAsecs += values.i;
			mVsecs += values.v;
			uint32_t mAh = mAsecs / 3600;
			uint32_t mWh = (mAh * (mVsecs / secs)) / 1000;

			DisplayElaplsedTime(secs);
			Lcd::Fill(0, 1, 96*5, Nokia::Clear);
			Lcd::SetXY(0, 2);
			Lcd::Puts(utoa(mWh, buf), font10x16);
			Lcd::Puts(" mWh\n\n");
			Lcd::Puts(utoa(mAh, buf), font10x16);
			Lcd::Puts(" mAh\n\n\n");
			Lcd::Fill(0, 7, 96, Nokia::Clear);
			Lcd::Puts(utoa(values.v, buf));
			Lcd::Puts("mV ");
			Lcd::Puts(utoa(values.i, buf));
			Lcd::Puts("mA");
		}
	}
}

void Putc(uint8_t ch)
{
	Usart::Putch(ch);
}
void main()
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
	Lcd::Init();
	Lcd::SetContrast(20);

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
			volatile uint16_t voltage = atoi((char*)str);
			if(voltage >= 500 && voltage <= 4200) Uthreshold = voltage;
			else
			{
				Usart::Puts("Value is not valid.");
				continue;
			}
			break;
		}
		while(true)
		{
			Usart::Puts("Enter discharge current in mA (in range 50...2000mA): ");
			while(!(str = Usarts::GetStr<Usart>()));
				;
			uint16_t current = atoi((char*)str);
			if(current >= 50 && current <= 2000) Itarget = current;
			else
			{
				Usart::Puts("Value is not valid.\r\n");
				continue;
			}
			break;
		}
		GreenLed::Clear();
		Tim1::Ccr1() = 4;
		SysTick_Config(100 * (F_CPU/(1000 * 8)), true);
		Lcd::Clear();
		while(!isFinished)
			;
		isFinished = false;
	}
}

