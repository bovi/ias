#!/usr/bin/env ruby

require 'serialport'

FLASH = true

USB_PORT = "ttyACM0"

# Building folder
BUILD_DIR = "/home/daniel/Documents/mruby_arduino_due/build"

# Arduino Application Folder
ARDUINO_DIR = "/home/daniel/Downloads/arduino-1.5.2"

# Standard Paths for build process
SAM_DIR = "#{ARDUINO_DIR}/hardware/arduino/sam"
BIN_DIR = "#{ARDUINO_DIR}/hardware/tools/g++_arm_none_eabi/bin"
TARGET_DIR = "#{SAM_DIR}/variants/arduino_due_x"
ARDUINO_SRC = "#{SAM_DIR}/cores/arduino"

# C Flags
CFLAGS_1 = "-c -g -Os -w -ffunction-sections -fdata-sections -nostdlib --param max-inline-insns-single=500"
CFLAGS_2 = "-fno-rtti -fno-exceptions" # Used for C++ files
CFLAGS_3 = "-Dprintf=iprintf -mcpu=cortex-m3 -DF_CPU=84000000L -DARDUINO=152 -D__SAM3X8E__ -mthumb -DUSB_PID=0x003e -DUSB_VID=0x2341 -DUSBCON"

INCLUDES = "-I#{SAM_DIR}/system/libsam -I#{SAM_DIR}/system/CMSIS/CMSIS/Include/ -I#{SAM_DIR}/system/CMSIS/Device/ATMEL/ -I#{SAM_DIR}/cores/arduino -I#{TARGET_DIR} -Imruby_src/include"

def execute cmd
  result = `#{cmd}`
  puts result unless result == ''
end

execute "rm -Rf #{BUILD_DIR}"
execute "mkdir #{BUILD_DIR}"
execute "cp -Rf mruby.cpp #{BUILD_DIR}/mruby.cpp"
execute "cp -Rf libmruby.a #{BUILD_DIR}/libmruby.a"

USER_FILES = %w(mruby.cpp)

C_FILES = %w(WInterrupts.c syscalls_sam3.c cortex_handlers.c wiring.c wiring_digital.c itoa.c wiring_shift.c wiring_analog.c hooks.c iar_calls_sam3.c)
#C_FILES = %w(syscalls_sam3.c cortex_handlers.c wiring.c wiring_digital.c wiring_shift.c wiring_analog.c hooks.c iar_calls_sam3.c)

CPP_FILES = %w(main.cpp WString.cpp RingBuffer.cpp UARTClass.cpp cxxabi-compat.cpp USARTClass.cpp USB/CDC.cpp USB/HID.cpp USB/USBCore.cpp Reset.cpp Stream.cpp Print.cpp WMath.cpp IPAddress.cpp wiring_pulse.cpp)
#CPP_FILES = %w(main.cpp RingBuffer.cpp UARTClass.cpp USARTClass.cpp USB/CDC.cpp USB/HID.cpp USB/USBCore.cpp Reset.cpp Print.cpp)

def add_to_lib file
  execute "#{BIN_DIR}/arm-none-eabi-ar rcs #{BUILD_DIR}/core.a #{BUILD_DIR}/#{file}"
end

puts "CC (User Files)"
# Userspecific Code
USER_FILES.each do |src_file|
  obj_file = "#{src_file}.o"
  execute "#{BIN_DIR}/arm-none-eabi-g++ #{CFLAGS_1} #{CFLAGS_2} #{CFLAGS_3} #{INCLUDES} #{BUILD_DIR}/#{src_file} -o #{BUILD_DIR}/#{obj_file}"
  add_to_lib obj_file
end

puts "CC (C Files)"
# Arduino Standard C Code
C_FILES.each do |src_file|
  obj_file = "#{src_file}.o"
  execute "#{BIN_DIR}/arm-none-eabi-gcc #{CFLAGS_1} #{CFLAGS_3} #{INCLUDES} #{ARDUINO_SRC}/#{src_file} -o #{BUILD_DIR}/#{obj_file}"
  add_to_lib obj_file
end

puts "CC (CPP Files)"
# Arduino Standard C++ Code
CPP_FILES.each do |src_file|
  obj_file = "#{src_file}.o"
  obj_file.sub! 'USB/', ''
  execute "#{BIN_DIR}/arm-none-eabi-g++ #{CFLAGS_1} #{CFLAGS_2} #{CFLAGS_3} #{INCLUDES} #{ARDUINO_SRC}/#{src_file} -o #{BUILD_DIR}/#{obj_file}"
  add_to_lib obj_file
end

puts "CC (variant)"
execute "#{BIN_DIR}/arm-none-eabi-g++ #{CFLAGS_1} #{CFLAGS_2} #{CFLAGS_3} #{INCLUDES} #{TARGET_DIR}/variant.cpp -o #{BUILD_DIR}/variant.cpp.o"
add_to_lib 'variant.cpp.o'

puts "LD"
# Link User specific things and Arduino Specific things together
execute "#{BIN_DIR}/arm-none-eabi-g++ -Os -Wl,--gc-sections -mcpu=cortex-m3 -T#{TARGET_DIR}/linker_scripts/gcc/flash.ld -Wl,-Map,#{BUILD_DIR}/mruby.cpp.map -o #{BUILD_DIR}/mruby.cpp.elf -L#{BUILD_DIR} -lm -lgcc -mthumb -Wl,--cref -Wl,--check-sections -Wl,--gc-sections -Wl,--entry=Reset_Handler -Wl,--unresolved-symbols=report-all -Wl,--warn-common -Wl,--warn-section-align -Wl,--warn-unresolved-symbols -Wl,--start-group #{BUILD_DIR}/libmruby.a #{BUILD_DIR}/syscalls_sam3.c.o #{BUILD_DIR}/mruby.cpp.o #{TARGET_DIR}/libsam_sam3x8e_gcc_rel.a #{BUILD_DIR}/core.a -Wl,--end-group"

# Build Binary for target
puts "PACK"
execute "#{BIN_DIR}/arm-none-eabi-objcopy -O binary #{BUILD_DIR}/mruby.cpp.elf #{BUILD_DIR}/mruby.cpp.bin"

if FLASH
  SerialPort.open("/dev/#{USB_PORT}", 1200) {|sp| puts "Reset Board" }

  # Upload to Board
  puts "Upload to Flash"
  result = `#{ARDUINO_DIR}/hardware/tools/bossac --port=#{USB_PORT} -U false -e -w -v -b #{BUILD_DIR}/mruby.cpp.bin -R`
  size = result.match(/Write (.*?) bytes to flash/)[1]
  puts result
end

=begin
t = ""
SerialPort.open("/dev/#{USB_PORT}", 9600) do |sp|
  10.times { sp.read; sleep 1 }
  10.times { t += sp.read; sleep 1}
end
t = t.split("\r\n")
t = t[4]

puts "#{size};#{t}#{ARGV[0]}"
`echo "#{size};#{t}#{ARGV[0]}" >> ./time.csv`
=end
