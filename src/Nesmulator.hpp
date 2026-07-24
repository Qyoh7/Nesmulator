#include <cstdlib>
#include <cstring>
#include <iostream>
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>

typedef uint8_t byte;
typedef uint16_t word;

class Nesmulator
{
    private:
        word programCounter; 
        byte stackPointer;
        unsigned int cycles; // CPU cycles taken to excecute an instruction
        byte A; // Math & Bitwise operations
        byte X; // Indexing & Counting
        byte Y; // Indexing & Counting

        byte tempByte;
        word addressBus;

        std::array<byte, 0x800> RAM; // Address $0000 to $07FF
        std::array<byte, 0x8000> ROM; // Address $8000 to $FFFF
        std::array<byte, 0x10> header; // the iNES header, first 16 bytes in the rom

        std::ifstream romFile;

        bool cpuHalted;

        // Status flags
        bool flagCarry;
        bool flagZero;
        bool flagInterruptDisable;
        bool flagDecimal;
        bool flagOverflow;
        bool flagNegative;

        void branch(byte offset)
        {
            // offset is a signed value
            int signedVal = offset;
            if (signedVal > 127)
            {
                signedVal -= 256;
            }
            programCounter = static_cast<word>(programCounter + signedVal);
        }

    public:
        Nesmulator(std::string romFile)
        {
            this->romFile.open(romFile, std::ios::in | std::ios::binary);
            if (!this->romFile.is_open()) 
            {
                std::cerr << "Failed to open ROM file\n";
                std::cerr << "Error: " << strerror(errno) << "\n";
            }
            cpuHalted = false;
            cycles = 0;
            stackPointer = 0xFD;

            addressBus = 0;
            tempByte = 0;

            flagCarry = false;
            flagZero = false;
            flagInterruptDisable = true;
            flagDecimal = false;
            flagOverflow = false;
            flagNegative = false;
        }

        void run()
        {
            while (!cpuHalted)
            {
                std::cout << logLine(read(programCounter)) << "\n";
                emulateCpu();
            }
        }

        byte read(word address)
        {
            if (address < 0x0800) return RAM[address];
            else if (address >= 0x8000) return ROM[address - 0x8000];
            else {
                std::cerr << "Unable to read, Address " << address << " is out of bounds!\n";
                exit(EXIT_FAILURE);
                return 0;
            }
        }

        void write(word address, byte value)
        {
            if (address < 0x800) RAM[address] = value;
            else {
                std::cerr << "Unable to write, Address " << address << " Is out of bounds!\n";
            }
        }

        void updateFlagsZN(byte& value)
        {
            flagZero = (value == 0);
            flagNegative = (value > 127);
        }

        void push(byte value)
        {
            write(static_cast<word>(0x100 + stackPointer), value);
            stackPointer--;
        }

        byte pull()
        {
            stackPointer++;
            return read(static_cast<word>(0x100 + stackPointer));
        }

        word pullWord()
        {
            byte low = pull();
            byte high = pull();
            return parseAddress(low, high);
        }

        word parseAddress(byte lowByte, byte highByte)
        {
            return static_cast<word>((highByte * 0x100) + lowByte);
        }
        
        byte readByte(bool incPC = true)
        {
            byte operand = read(programCounter);
            if (incPC) programCounter++;
            return operand;
        }

        word readWord(bool incPC = true)
        {
            byte low = read(programCounter++);
            byte high = read(programCounter);
            if (incPC) programCounter++;
            return parseAddress(low, high);
        }

        byte readOperandsZeroPage()
        {
            addressBus = readByte();
            return read(addressBus);
        }

        byte readOperandsZeroPageIndexed(byte reg)
        {
            addressBus = readByte();
            byte ret = read(addressBus);
            addressBus += reg;
            if (addressBus > 0xFF) addressBus -= 0x100;
            return ret;
        }

        byte readOperandsAbsolute()
        {
            addressBus = readWord();
            return read(addressBus);
        }

        byte readOperandsAbsoluteIndexed(byte reg)
        {
            addressBus = readWord();
            byte ret = read(addressBus);
            addressBus += reg;
            return ret;
        }

        byte readOperandsIndirectIndexed(byte reg)
        {
            addressBus = readByte();
            read(addressBus);
        }

        byte opLSR(byte value)
        {
            flagCarry = (value & 0b1);
            byte shifted = value >>= 1;
            updateFlagsZN(shifted);
            return shifted;
        }

        byte opASL(byte value)
        {
            flagCarry = (value & 0b10000000);
            byte shifted = value <<= 1;
            updateFlagsZN(shifted);
            return shifted;
        }

        byte opROL(byte value)
        {
            bool oldCarry = flagCarry;
            flagCarry = (value & 0b10000000);
            value = (value << 1) | oldCarry;
            updateFlagsZN(value);
            return value;
        }

        byte opROR(byte value)
        {
            bool oldCarry = flagCarry;
            flagCarry = (value & 0b00000001);
            value = (value >> 1) | (oldCarry << 7);
            updateFlagsZN(value);
            return value;
        }

        byte opINC(byte value)
        {
            byte val = value++;
            updateFlagsZN(value);
            return val;
        }

        byte opORA(byte value)
        {
            byte ret = A | value;
            updateFlagsZN(ret);
            return ret;
        }

        byte opAND(byte value)
        {
            byte ret = A & value;
            updateFlagsZN(ret);
            return ret;
        }

        byte opEOR(byte value)
        {
            byte ret = A ^ value;
            updateFlagsZN(ret);
            return ret;
        }

        byte opADC(byte value)
        {
            int sum = A + value + (flagCarry ? 1 : 0);
            flagCarry = sum > 0xFF;
            flagOverflow = (~(A ^ value) & (A ^ sum) & 0x80) != 0;
            byte ret = static_cast<byte>(sum);
            updateFlagsZN(ret);
            return ret;
        }

        byte opSBC(byte value)
        {
            int sum = A - value - (flagCarry ? 0 : 1);
            flagCarry = sum >= 0x0;
            flagOverflow = ((A ^ value) & (A ^ sum) & 0x80) != 0;
            byte ret = static_cast<byte>(sum);
            updateFlagsZN(ret);
            return ret;
        }

        void opCMP(byte value)
        {
            flagCarry = A >= value;
            flagZero = A == value;
            flagNegative = (A - value) < 0;
        }

        void opCPX(byte value)
        {
            flagCarry = X >= value;
            flagZero = X == value;
            flagNegative = (X - value) < 0;
        }

        void opCPY(byte value)
        {
            flagCarry = Y >= value;
            flagZero = Y == value;
            flagNegative = (Y - value) < 0;
        }

        void opBIT(byte value)
        {
            flagZero = (A | value) == 0;
            flagNegative = (value & 0b10000000) != 0;
            flagOverflow = (value & 0b01000000) != 0;
        }

        void emulateCpu()
        {
            byte opCode = read(programCounter);
            byte tempLow;
            byte tempHigh;
            programCounter++;
            switch (opCode)
            {
                case 0x00: // BRK
                    programCounter++;
                    push(static_cast<byte>(programCounter >> 8));
                    push(static_cast<byte>(programCounter));
                    tempByte = 0;
                    tempByte |= (flagCarry ? 0b00000001 : 0b0); 
                    tempByte |= (flagZero ? 0b00000010 : 0b0); 
                    tempByte |= (flagInterruptDisable ? 0b00000100 : 0b0); 
                    tempByte |= (flagDecimal ? 0b00001000 : 0b0);
                    tempByte |= 0b00010000; // Break (pushes 1 because this is a PHP instruction)
                    tempByte |= 0b00100000; // Who knows dawg
                    tempByte |= (flagOverflow ? 0b01000000 : 0b0);
                    tempByte |= (flagNegative ? 0b10000000 : 0b0);
                    push(tempByte);
                    tempLow = read(0xFFFE);
                    tempHigh = read(0xFFFF);
                    programCounter = static_cast<word>((tempHigh * 0x100) + tempLow);
                    cycles = 7;
                    break;

                case 0x02: // HLT
                    cpuHalted = true;
                    break;
                
                case 0x05: // ORA Zero Page
                    A = opORA(readOperandsZeroPage());
                    cycles = 3;
                    break;

                case 0x06: // ASL Zero page
                    tempByte = readOperandsZeroPage();
                    write(addressBus, opASL(tempByte));
                    cycles = 5;
                    break;

                case 0x08: // PHP
                    tempByte = 0;
                    tempByte |= (flagCarry ? 0b00000001 : 0b0); 
                    tempByte |= (flagZero ? 0b00000010 : 0b0); 
                    tempByte |= (flagInterruptDisable ? 0b00000100 : 0b0); 
                    tempByte |= (flagDecimal ? 0b00001000 : 0b0);
                    tempByte |= 0b00010000; // Break (pushes 1 because this is a PHP instruction)
                    tempByte |= 0b00100000; // Who knows dawg
                    tempByte |= (flagOverflow ? 0b01000000 : 0b0);
                    tempByte |= (flagNegative ? 0b10000000 : 0b0);
                    push(tempByte);
                    cycles = 3;
                    break;
                
                case 0x09: // ORA Immediate
                    A = opORA(readByte());
                    cycles = 2;
                    break;

                case 0x0A: // ASL A
                    A = opASL(A);
                    cycles = 2;
                    break;
                
                case 0x0D: // ORA Absolute
                    A = opORA(readOperandsAbsolute());
                    cycles = 4;
                    break;

                case 0x0E: // ASL Absolute
                    write(addressBus, opASL(readOperandsAbsolute()));
                    cycles = 6;
                    break;

                case 0x10: // BPL
                    tempByte = readByte();
                    if (!flagNegative)
                    {
                        branch(tempByte);
                        cycles = 3;
                    }
                    else {
                        cycles = 2;
                    }
                    break;

                case 0x18: // CLC
                    flagCarry = false;
                    cycles = 2;
                    break;

                case 0x20: // JSR
                    addressBus = readWord(false);
                    push(static_cast<byte>(programCounter / 0x100)); // Push PCH
                    push(static_cast<byte>(programCounter)); // Push PCL
                    programCounter = addressBus;
                    cycles = 6;
                    break;

                case 0x24: // BIT Zero Page
                    opBIT(readOperandsZeroPage());
                    cycles = 3;
                    break;

                case 0x25: // AND Zero Page
                    A = opAND(readOperandsZeroPage());
                    cycles = 3;
                    break;

                case 0x26: // ROL Zero Page
                    tempByte = readOperandsZeroPage();
                    write(addressBus, opROL(tempByte));
                    cycles = 5;
                    break;

                case 0x28: // PLP
                    tempByte = pull();
                    flagCarry =            (tempByte & 0b00000001);
                    flagZero =             (tempByte & 0b00000010);
                    flagInterruptDisable = (tempByte & 0b00000100);
                    flagDecimal =          (tempByte & 0b00100000);
                    flagOverflow =         (tempByte & 0b01000000);
                    flagNegative =         (tempByte & 0b10000000);
                    cycles = 3;
                    break;
                
                case 0x29: // AND Immediate
                    A = opAND(readByte());
                    cycles = 2;
                    break;

                case 0x2A: // ROL A
                    A = opROL(A);
                    cycles = 2;
                    break;

                case 0x2C: // BIT Absolute
                    opBIT(readOperandsAbsolute());
                    cycles = 4;
                    break;

                case 0x2D: // AND Absolute
                    A = opAND(readOperandsAbsolute());
                    cycles = 4;
                    break;

                case 0x2E: // ROL Absolute
                    tempByte = readOperandsAbsolute();
                    write(addressBus, opROL(tempByte));
                    cycles = 6;
                    break;

                case 0x30: // BMI
                    tempByte = readByte();
                    if (flagNegative)
                    {
                        branch(tempByte);
                        cycles = 3;
                    }
                    else {
                        cycles = 2;
                    }
                    break;

                case 0x38: // SEC
                    flagCarry = true;
                    cycles = 2;
                    break;

                case 0x40: // RTI
                    tempByte = pull();
                    flagCarry =            (tempByte & 0b00000001);
                    flagZero =             (tempByte & 0b00000010);
                    flagInterruptDisable = (tempByte & 0b00000100);
                    flagDecimal =          (tempByte & 0b00100000);
                    flagOverflow =         (tempByte & 0b01000000);
                    flagNegative =         (tempByte & 0b10000000);
                    programCounter = pullWord();
                    cycles = 6;
                    break;

                case 0x45: // EOR Zero Page
                    A = opEOR(readOperandsZeroPage());
                    cycles = 3;
                    break;

                case 0x46: // LSR Zero Page
                    tempByte = readOperandsZeroPage();
                    write(addressBus, opLSR(tempByte));
                    cycles = 5;
                    break;

                case 0x48: // PHA
                    push(A);
                    cycles = 3;
                    break;

                case 0x49: // EOR Immediate
                    A = opEOR(readByte());
                    cycles = 2;
                    break;

                case 0x4A: // LSR A
                    A = opLSR(A);
                    cycles = 2;
                    break;

                case 0x4C: // JMP Absolute
                    addressBus = readWord(false);
                    programCounter = addressBus;
                    cycles = 3;
                    break;

                case 0x4D: // EOR Absolute
                    A = opEOR(readOperandsAbsolute());
                    cycles = 4;
                    break;
                
                case 0x4E: // LSR Absolute
                    tempByte = readOperandsAbsolute();
                    write(addressBus, opLSR(tempByte));
                    cycles = 6;
                    break;

                case 0x50: // BVC
                    tempByte = readByte();
                    if (!flagOverflow)
                    {
                        branch(tempByte);
                        cycles = 3;
                    }
                    else {
                        cycles = 2;
                    }
                    break;

                case 0x58: // CLI
                    flagInterruptDisable = false;
                    cycles = 2;
                    break;

                case 0x60: // RTS
                    programCounter = pullWord();
                    programCounter++;
                    cycles = 6;
                    break;

                case 0x65: // ADC Zero Page
                    A = opADC(readOperandsZeroPage());
                    cycles = 3;
                    break;
                
                case 0x66: // ROR Zero Page
                    tempByte = readOperandsZeroPage();
                    write(addressBus, opROR(tempByte));
                    cycles = 5;
                    break;

                case 0x68: // PLA
                    A = pull();
                    updateFlagsZN(A);
                    cycles = 4;
                    break;

                case 0x69: // ADC Immediate
                    A = opADC(readByte());
                    cycles = 2;
                    break;

                case 0x6A: // ROR A
                    A = opROR(A);
                    cycles = 2;
                    break;

                case 0x6C: // JMP Indirect
                    programCounter = readWord();
                    programCounter = readWord();
                    cycles = 5;
                    break;

                case 0x6D: // ADC Absolute
                    A = opADC(readOperandsAbsolute());
                    cycles = 4;
                    break;
                
                case 0x6E: // ROR Absolute
                    tempByte = readOperandsAbsolute();
                    write(addressBus, opROR(tempByte));
                    cycles = 6;
                    break;

                case 0x70: // BVS
                    tempByte = readByte();
                    if (flagOverflow)
                    {
                        branch(tempByte);
                        cycles = 3;
                    }
                    else {
                        cycles = 2;
                    }
                    break;

                case 0x78: // SEI
                    flagInterruptDisable = true;
                    cycles = 2;
                    break;

                case 0x84: // STY Zero Page
                    write(readByte(), Y);
                    cycles = 3;
                    break;

                case 0x85: // STA Zero Page
                    tempByte = readByte();
                    write(tempByte, A);
                    cycles = 3;
                    break;

                case 0x86: // STX Zero Page
                    tempByte = readByte();
                    write(tempByte, X);
                    cycles = 3;
                    break;

                case 0x88: // DEY
                    Y--;
                    updateFlagsZN(Y);
                    cycles = 2;
                    break;

                case 0x8A: // TXA
                    A = X;
                    updateFlagsZN(A);
                    cycles = 2;
                    break;

                case 0x8C: // STY Absolute
                    addressBus = readWord();
                    write(addressBus, Y);
                    cycles = 4;
                    break;

                case 0x8D: // STA Absolute
                    addressBus = readWord();
                    write(addressBus, A);
                    cycles = 4;
                    break;

                case 0x8E: // STX Absolute
                    addressBus = readWord();
                    write(addressBus, X);
                    cycles = 4;
                    break;

                case 0x90: // BCC
                    tempByte = read(programCounter);
                    programCounter++;
                    if (!flagCarry)
                    {
                        branch(tempByte);
                        cycles = 3;
                    }
                    else {
                        cycles = 2;
                    }
                    break;

                case 0x98: // TYA
                    A = Y;
                    updateFlagsZN(A);
                    cycles = 2;
                    break;

                case 0x9A: // TXS
                    stackPointer = static_cast<word>(X);
                    cycles = 2;
                    break;

                case 0xA0: // LDY Immediate
                    Y = readByte();
                    updateFlagsZN(Y);
                    cycles = 2;
                    break;

                case 0xA2: // LDX Immediate
                    X = readByte();
                    updateFlagsZN(X);
                    cycles = 2;
                    break;

                case 0xA5: // LDA Zero Page
                    A = read(readByte());
                    updateFlagsZN(A);
                    cycles = 3;
                    break;

                case 0xA8: // TAY
                    Y = A;
                    updateFlagsZN(Y);
                    cycles = 2;
                    break;

                case 0xA9: // LDA Immediate
                    A = readByte();
                    updateFlagsZN(A);
                    cycles = 2;
                    break;

                case 0xAA: // TAX
                    X = A;
                    updateFlagsZN(X);
                    cycles = 2;
                    break;

                case 0xAD: // LDA Absolute
                    A = read(readWord());
                    updateFlagsZN(A);
                    break;

                case 0xB0: // BCS
                    tempByte = read(programCounter);
                    programCounter++;
                    if (flagCarry)
                    {
                        branch(tempByte);
                        cycles = 3;
                    }
                    else {
                        cycles = 2;
                    }
                    break;

                case 0xB8: // CLV
                    flagOverflow = false;
                    cycles = 2;
                    break;

                case 0xBA: // TSX
                    X = static_cast<byte>(stackPointer);
                    updateFlagsZN(X);
                    cycles = 2;
                    break;

                case 0xC0: // CPY Immediate
                    opCPY(readByte());
                    cycles = 2;
                    break;
                
                case 0xC4: // CPY Zero Page
                    opCPY(readOperandsZeroPage());
                    cycles = 3;
                    break;

                case 0xC5: // CMP Zero Page
                    opCMP(readOperandsZeroPage());
                    cycles = 3;
                    break;

                case 0xC8: // INY
                    Y++;
                    updateFlagsZN(Y);
                    cycles = 2;
                    break; 

                case 0xC9: // CMP Immediate
                    opCMP(readByte());
                    cycles = 2;
                    break;

                case 0xCA: // DEX
                    X--;
                    updateFlagsZN(X);
                    cycles = 2;
                    break;

                case 0xCC: // CPY Absolute
                    opCPY(readOperandsAbsolute());
                    cycles = 4;
                    break;

                case 0xCD: // CMP Absolute
                    opCMP(readOperandsAbsolute());
                    cycles = 4;
                    break;

                case 0xD0: // BNE
                    tempByte = read(programCounter);
                    programCounter++;
                    if (!flagZero)
                    {
                        branch(tempByte);
                        cycles = 3;
                    }
                    else {
                        cycles = 2;
                    }
                    break;

                case 0xD8: // CLD
                    flagDecimal = false;
                    cycles = 2;
                    break;

                case 0xE0: // CPX Immediate
                    opCPX(readByte());
                    cycles = 2;
                    break;

                case 0xE4: // CPX Zero Page
                    opCPX(readOperandsZeroPage());
                    cycles = 3;
                    break;

                case 0xE5: // SBC Zero Page
                    A = opSBC(readOperandsZeroPage());
                    cycles = 3;
                    break;

                case 0xE6: // INC Zero Page
                    tempByte = readOperandsZeroPage();
                    write(addressBus, opINC(tempByte));
                    cycles = 5;
                    break;

                case 0xE8: // INX (implied)
                    X++;
                    updateFlagsZN(X);
                    cycles = 2;
                    break;

                case 0xE9: // SBC Immediate
                    A = opSBC(readByte());
                    cycles = 2;
                    break;

                case 0xEA: // NOP
                    cycles = 2;
                    break;

                case 0xEC: // CPX Absolute
                    opCPX(readOperandsAbsolute());
                    cycles = 4;
                    break;

                case 0xED: // SBC Absolute
                    A = opSBC(readOperandsAbsolute());
                    cycles = 4;
                    break;

                case 0xEE: // INC Absolute
                    tempByte = readOperandsAbsolute();
                    write(addressBus, tempByte);
                    cycles = 6;
                    break;

                case 0xF0: // BEQ
                    tempByte = readByte();
                    if (flagZero)
                    {
                        branch(tempByte);
                        cycles = 3;
                    }
                    else {
                        cycles = 2;
                    }
                    break;

                case 0xF8: // SED
                    flagDecimal = true;
                    cycles = 2;
                    break;

                default:
                    std::cerr << "Invalid opcode: $" << hex8(opCode) << "\n";
                    break;
            }
        }

        void reset()
        {
            // We read the romfile into an array, throw the iNES header in another array, then 
            // throw the rest of the romfile into ROM
            std::array<byte, 0x8010> headeredRom;
            romFile.read(reinterpret_cast<char*>(headeredRom.data()), 0x8010);
            std::copy(headeredRom.begin(), headeredRom.begin() + 0x10, header.begin());
            std::copy(headeredRom.begin() + 0x10, headeredRom.end(), ROM.begin());
            
            // find the freakign PC (last 6 bytes of the ROM)
            byte PCL = read(0xFFFC); // PC low byte
            byte PCH = read(0xFFFD); // PC high byte

            // Since PC is in little endian, shift the high byte two decimal places to the left, then add the low byte. Neat!
            programCounter = static_cast<word>((PCH * 0x100) + PCL);

            run();
        }

        std::string hex8(byte b)
        {
            std::stringstream ss;
            ss << std::uppercase
               << std::hex
               << std::setw(2)
               << std::setfill('0')
               << static_cast<int>(b);
            return ss.str();
        }

        std::string hex16(word w)
        {
            std::stringstream ss;
            ss << std::uppercase
               << std::hex
               << std::setw(4)
               << std::setfill('0')
               << static_cast<int>(w);
            return ss.str();
        }

        std::string logLine(byte opCode)
        {
            const std::array<std::string, 256> opcodeNames = {
                "BRK","ORA","KIL","SLO","NOP","ORA","ASL","SLO","PHP","ORA","ASL","ANC","NOP","ORA","ASL","SLO",
                "BPL","ORA","KIL","SLO","NOP","ORA","ASL","SLO","CLC","ORA","NOP","SLO","NOP","ORA","ASL","SLO",
                "JSR","AND","KIL","RLA","BIT","AND","ROL","RLA","PLP","AND","ROL","ANC","BIT","AND","ROL","RLA",
                "BMI","AND","KIL","RLA","NOP","AND","ROL","RLA","SEC","AND","NOP","RLA","NOP","AND","ROL","RLA",
                "RTI","EOR","KIL","SRE","NOP","EOR","LSR","SRE","PHA","EOR","LSR","ALR","JMP","EOR","LSR","SRE",
                "BVC","EOR","KIL","SRE","NOP","EOR","LSR","SRE","CLI","EOR","NOP","SRE","NOP","EOR","LSR","SRE",
                "RTS","ADC","KIL","RRA","NOP","ADC","ROR","RRA","PLA","ADC","ROR","ARR","JMP","ADC","ROR","RRA",
                "BVS","ADC","KIL","RRA","NOP","ADC","ROR","RRA","SEI","ADC","NOP","RRA","NOP","ADC","ROR","RRA",
                "NOP","STA","NOP","SAX","STY","STA","STX","SAX","DEY","NOP","TXA","XAA","STY","STA","STX","SAX",
                "BCC","STA","KIL","AHX","STY","STA","STX","SAX","TYA","STA","TXS","TAS","SHY","STA","SHX","AHX",
                "LDY","LDA","LDX","LAX","LDY","LDA","LDX","LAX","TAY","LDA","TAX","LAX","LDY","LDA","LDX","LAX",
                "BCS","LDA","KIL","LAX","LDY","LDA","LDX","LAX","CLV","LDA","TSX","LAS","LDY","LDA","LDX","LAX",
                "CPY","CMP","NOP","DCP","CPY","CMP","DEC","DCP","INY","CMP","DEX","AXS","CPY","CMP","DEC","DCP",
                "BNE","CMP","KIL","DCP","NOP","CMP","DEC","DCP","CLD","CMP","NOP","DCP","NOP","CMP","DEC","DCP",
                "CPX","SBC","NOP","ISC","CPX","SBC","INC","ISC","INX","SBC","NOP","SBC","CPX","SBC","INC","ISC",
                "BEQ","SBC","KIL","ISC","NOP","SBC","INC","ISC","SED","SBC","NOP","ISC","NOP","SBC","INC","ISC"
            };
            std::stringstream logLine; 
            logLine << hex16(programCounter) << "    ";
            logLine << hex8(opCode) << "    ";
            logLine << opcodeNames[opCode] << "   ";
            logLine << "A: " << hex8(A) << "    ";
            logLine << "X: " << hex8(X) << "    ";
            logLine << "Y: " << hex8(Y) << "    ";
            logLine << "SP: " << hex8(stackPointer) << "    ";
            logLine << (flagNegative ? "N" : "n");
            logLine << (flagOverflow ? "V" : "v");
            logLine << "--";
            logLine << (flagDecimal ? "D" : "d");
            logLine << (flagInterruptDisable ? "I" : "i");
            logLine << (flagCarry ? "C" : "c");
            return logLine.str();
        }
        void test1()
        {
            reset();
        }

        void test2()
        {
            reset();
            std::cout << "\n" << "=====TEST RESULTS=====\n";
            std::cout << "A register: " << std::hex << static_cast<int>(A) << "\n";
            std::cout << "X register: " << std::hex << static_cast<int>(X) << "\n";
            std::cout << "Y register: " << std::hex << static_cast<int>(Y) << "\n";
            std::cout << "Address 0x0000 (should read $5A): " << std::hex << static_cast<int>(read(0x0)) << "\n";
            std::cout << "Address 0x0001 (should read $5A): " << std::hex << static_cast<int>(read(0x0001)) << "\n";
            std::cout << "Address 0x0002 (should read $80): " << std::hex << static_cast<int>(read(0x0002)) << "\n";
            std::cout << "Address 0x0550 (should read $80): " << std::hex << static_cast<int>(read(0x0550)) << "\n";
        }
        void test3()
        {
            reset();
            std::cout << "\n" << "=====TEST RESULTS=====\n";
            std::cout << "Address 0x0000 (should read $01) " << std::hex << static_cast<int>(read(0x0)) << "\n";
            std::cout << "Final PC Location (should be $8017) " << std::hex << static_cast<int>(programCounter) << "\n";
        }

        void test4()
        {
            reset();
            std::cout << "\n" << "=====TEST RESULTS=====\n";
            std::cout << "Address 0x0000 (should read $01) " << std::hex << static_cast<int>(read(0x0)) << "\n";
        }
        
        void test5()
        {
            reset();
            std::cout << "\n" << "=====TEST RESULTS=====\n";

            std::cout << "Address 0x0000 (should read $02) " << hex8(read(0x0)) << "\n";
            std::cout << "Address 0x0001 (should read $01) " << hex8(read(0x1)) << "\n";
            std::cout << "Address 0x0002 (should read $FD) " << hex8(read(0x2)) << "\n";
            std::cout << "Address 0x0003 (should read $3D) " << hex8(read(0x3)) << "\n";
            std::cout << "Address 0x0004 (should read $34) " << hex8(read(0x4)) << "\n";
            std::cout << "Address 0x0005 (should read $05) " << hex8(read(0x5)) << "\n";
            std::cout << "Address 0x0006 (should read $53) " << hex8(read(0x6)) << "\n";
            std::cout << "Address 0x0007 (should read $11) " << hex8(read(0x7)) << "\n";
            std::cout << "Address 0x0008 (should read $90) " << hex8(read(0x8)) << "\n";
            std::cout << "Address 0x0009 (should read $F0) " << hex8(read(0x9)) << "\n";
            std::cout << "Address 0x000A (should read $70) " << hex8(read(0xA)) << "\n";
            std::cout << "Address 0x000B (should read $01) " << hex8(read(0xB)) << "\n";
            std::cout << "Address 0x000C (should read $01) " << hex8(read(0xC)) << "\n";
        }

};
