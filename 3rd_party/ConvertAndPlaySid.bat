.\PSID\psid64.exe %1 --output output.prg
.\ef3utils\ef3usb.exe COM4 s .\output.prg
del output.prg