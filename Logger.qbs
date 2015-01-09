import qbs

CppApplication
{
	property string platform: "f030"
	consoleApplication: true
	cpp.positionIndependentCode: false
	cpp.executableSuffix: ".elf"
	cpp.optimization: "small"
	cpp.debugInformation: false
	cpp.defines: [ "STM32F030" ]
	property stringList commonLinkerCFlags:
	[
		"-mthumb",
		"-mcpu=cortex-m0.small-multiply",
		"-mtune=cortex-m0.small-multiply",
		"-flto"
		//"-nostdlib", "-nodefaultlibs"
	]
	property stringList commonLinkerCppFlags:
	[
		"-fno-rtti",
		"-fno-exceptions",
		"-fno-threadsafe-statics",
	].concat(commonLinkerCFlags)
	cpp.commonCompilerFlags:
	[
		"-fdata-sections",
		"-ffunction-sections",
		"-fshort-enums",
		"-ffreestanding",
		"--specs=nano.specs",
		"-mno-lra"
	]
	cpp.cFlags: ["-std=c11"].concat(commonLinkerCFlags)
	cpp.cxxFlags: ["-std=c++14"].concat(commonLinkerCppFlags)
	cpp.linkerFlags:
	[
		"-nostartfiles",
		"-Wl,--gc-sections",
		"-lnosys","-lgcc","-lc", "-lm"
//		"-v", "-Wl,--Map=c:/Projects/output.map", "-lstdc++"
	].concat(commonLinkerCFlags, commonLinkerCppFlags)
	cpp.linkerScripts: platform + "/Linker.ld "
	cpp.includePaths:
	[
		platform + "/",
		"mcu_common/",
		"stm32f0_hal/",
		"stm32f0_hal/CMSIS/CoreSupport/",
		"stm32f0_hal/CMSIS/DeviceSupport/",
		"include/",
		"source/"
	]
	files:
	[
		platform + "/Startup.S",
		"**/*.hpp", "**/*.cpp", "**/*.h",  "**/*.c"
	]

}


