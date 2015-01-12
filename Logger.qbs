import qbs

CppApplication
{
	property string platform: "f030"
	consoleApplication: true
	cpp.positionIndependentCode: false
	cpp.executableSuffix: ".elf"
	cpp.optimization: "small"
	cpp.debugInformation: false
	cpp.defines: [ "STM32F030", "DONTSETCLOCK" ]
    property stringList commonFlags:
    [
        "-mcpu=cortex-m0",
        "-mtune=cortex-m0",
        "-mthumb",
        "-flto",
        "-Wno-vla"
        //"-nostdlib", "-nodefaultlibs"
    ]
	property stringList commonLinkerCppFlags:
	[
		"-fno-rtti",
		"-fno-exceptions",
		"-fno-threadsafe-statics",
    ]
	cpp.commonCompilerFlags:
	[
        "-fdata-sections",
		"-ffunction-sections",
		"-fshort-enums",
		"-ffreestanding",
        "--specs=nano.specs",
        "-mno-lra"
    ].concat(commonFlags)
    cpp.cFlags: ["-std=c11"].concat(commonFlags)
    cpp.cxxFlags: ["-std=c++14"].concat(commonLinkerCppFlags)
	cpp.linkerFlags:
	[
		"-nostartfiles",
		"-Wl,--gc-sections",
		"-lnosys","-lgcc","-lc", "-lm"
//		"-v", "-Wl,--Map=c:/Projects/output.map", "-lstdc++"
    ].concat(commonLinkerCppFlags, commonFlags)
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


