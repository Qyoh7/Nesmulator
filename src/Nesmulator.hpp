#include <cstring>
#include <iostream>
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>

typedef uint8_t byte;


class Nesmulator
{
    private:
        // its 1am
        bool yeah = true;
        bool nah = false;

        unsigned short programCounter; 
        byte stackPointer;
        unsigned int cycles; // CPU cycles taken to excecute an instruction
        byte A; // Math & Bitwise operations
        byte X; // Indexing & Counting
        byte Y; // Indexing & Counting

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
            programCounter = static_cast<unsigned short>(programCounter + signedVal);
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

            flagCarry = false;
            flagZero = false;
            flagInterruptDisable = true;
            flagDecimal = false;
            flagOverflow = false;
            flagNegative = false;
        }

        byte read(unsigned short address)
        {
            if (address < 0x0800) return RAM[address];
            else if (address >= 0x8000) return ROM[address - 0x8000];
            else {
                std::cerr << "Unable to read, Address " << address << " is out of bounds!\n";
                return 0;
            }
        }

        void write(unsigned short address, byte value)
        {
            if (address < 0x800) RAM[address] = value;
            else {
                std::cerr << "Unable to write, Address " << address << " Is out of bounds!\n";
            }
        }

        void push(byte value)
        {
            write(static_cast<unsigned short>(0x100 + stackPointer), value);
            stackPointer--;
        }

        byte pull()
        {
            stackPointer++;
            return read(static_cast<unsigned short>(0x100 + stackPointer));
        }

        void run()
        {
            while (!cpuHalted)
            {
                emulateCpu();
            }
        }
        void updateFlags(byte& reg)
        {
            flagZero = (reg == 0);
            flagNegative = (reg > 127);
        }

        // all the things

        unsigned short parseAddress(byte lowByte, byte highByte)
        {
            return static_cast<unsigned short>((highByte * 0x100) + lowByte);
        }
        
        unsigned short readOperandsAbsolute(bool incPC = true)
        {
            byte tempLow;
            byte tempHigh;
            tempLow = read(programCounter);
            programCounter++;
            tempHigh = read(programCounter);
            if (incPC) programCounter++;
            return parseAddress(tempLow, tempHigh);
        }

        byte readOperandsZeroPage(bool incPC = true)
        {
            byte operand;
            operand = read(programCounter);
            if (incPC) programCounter++;
            return operand;
        }

        void emulateCpu()
        {
            byte opCode = read(programCounter);
            byte tempByte;
            byte tempAddress;
            programCounter++;
            switch (opCode)
            {
                case 0x02: // HLT
                    cpuHalted = true;
                    break;

                case 0x06: // ASL (zero page)
                    
                    break;
                case 0x08: // PHP
                    tempByte = 0;
                    tempByte |= (flagCarry ? 0b00000001 : 0b0); 
                    tempByte |= (flagZero ? 0b00000010 : 0b0); 
                    tempByte |= (flagInterruptDisable ? 0b00000100 : 0b0); 
                    tempByte |= (flagDecimal ? 0b00001000 : 0b0);
                    tempByte |= 0b00010000; // Break (pushes 1 because this is a PHP instruction)
                    tempByte |= 0b00100001; // Who fuckin knows dawg
                    tempByte |= (flagOverflow ? 0b01000000 : 0b0);
                    tempByte |= (flagNegative ? 0b10000000 : 0b0);
                    push(tempByte);
                    cycles = 3;
                    break;

                case 0x0A: // ASL A
                    flagCarry = (A & 0b10000000) == 1;
                    A <<= 1;
                    updateFlags(A);
                    cycles = 2;
                    break;

                case 0x10: // BPL
                    // temp is a signed value
                    tempByte = read(programCounter);
                    programCounter++;
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
                    tempAddress = readOperandsAbsolute(false);
                    push(static_cast<byte>(programCounter / 0x100)); // Push PCH
                    push(static_cast<byte>(programCounter)); // Push PCL
                    programCounter = tempAddress;
                    cycles = 6;
                    break;

                case 0x28: // PLP
                    tempByte = pull();
                    flagCarry =            (tempByte & 0b00000001) == 1;
                    flagZero =             (tempByte & 0b00000010) == 1;
                    flagInterruptDisable = (tempByte & 0b00000100) == 1;
                    flagDecimal =          (tempByte & 0b00100000) == 1;
                    flagOverflow =         (tempByte & 0b01000000) == 1;
                    flagNegative =         (tempByte & 0b10000000) == 1;
                    cycles = 3;
                    break;

                case 0x30: // BMI
                    // temp is a signed value
                    tempByte = read(programCounter);
                    programCounter++;
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

                case 0x48: // PHA
                    push(A);
                    cycles = 3;
                    break;

                case 0x4C: // JMP (absolute)
                    tempAddress = readOperandsAbsolute(false);
                    programCounter = tempAddress;
                    cycles = 3;
                    break;

                case 0x50: // BVC
                    tempByte = read(programCounter);
                    programCounter++;
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
                    programCounter = parseAddress(pull(), pull());
                    programCounter++;
                    cycles = 6;
                    break;

                case 0x68: // PLA
                    A = pull();
                    updateFlags(A);
                    cycles = 4;
                    break;

                case 0x70: // BVS
                    tempByte = read(programCounter);
                    programCounter++;
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
                    tempByte = readOperandsZeroPage();
                    write(tempByte, Y);
                    cycles = 3;
                    break;

                case 0x85: // STA Zero Page
                    tempByte = readOperandsZeroPage();
                    write(tempByte, A);
                    cycles = 3;
                    break;

                case 0x86: // STX Zero Page
                    tempByte = readOperandsZeroPage();
                    write(tempByte, X);
                    cycles = 3;
                    break;

                case 0x88: // DEY
                    Y--;
                    updateFlags(Y);
                    cycles = 2;
                    break;

                case 0x8A: // TXA
                    A = X;
                    updateFlags(A);
                    cycles = 2;
                    break;

                case 0x8C: // STY Absolute
                    tempAddress = readOperandsAbsolute();
                    write(tempAddress, Y);
                    cycles = 4;
                    break;

                case 0x8D: // STA Absolute
                    tempAddress = readOperandsAbsolute();
                    write(tempAddress, A);
                    cycles = 4;
                    break;

                case 0x8E: // STX Absolute
                    tempAddress = readOperandsAbsolute();
                    write(tempAddress, X);
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
                    updateFlags(A);
                    cycles = 2;
                    break;

                case 0x9A: // TXS
                    stackPointer = static_cast<unsigned short>(X);
                    cycles = 2;
                    break;

                case 0xA0: // LDY Immediate
                    Y = readOperandsZeroPage();
                    updateFlags(Y);
                    cycles = 2;
                    break;

                case 0xA2: // LDX Immediate
                    X = readOperandsZeroPage();
                    updateFlags(X);
                    cycles = 2;
                    break;

                case 0xA5: // LDA Zero Page
                    A = readOperandsZeroPage();
                    updateFlags(A);
                    cycles = 3;
                    break;

                case 0xA8: // TAY
                    Y = A;
                    updateFlags(Y);
                    cycles = 2;
                    break;

                case 0xA9: // LDA Immediate
                    A = readOperandsZeroPage();
                    updateFlags(A);
                    cycles = 2;
                    break;

                case 0xAA: // TAX
                    X = A;
                    updateFlags(X);
                    cycles = 2;
                    break;

                case 0xAD: // LDA Absolute
                    A = readOperandsAbsolute();
                    updateFlags(A);
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
                    updateFlags(X);
                    cycles = 2;
                    break;

                case 0xC8: // INY
                    Y++;
                    updateFlags(Y);
                    cycles = 2;
                    break; 

                case 0xCA: // DEX
                    X--;
                    updateFlags(X);
                    cycles = 2;
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

                case 0xE8: // INX (implied)
                    X++;
                    updateFlags(X);
                    cycles = 2;
                    break;

                case 0xEA: // NOP
                    cycles = 2;
                    break;

                case 0xF0: // BEQ
                    tempByte = read(programCounter);
                    programCounter++;
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
                    std::cerr << "Invalid opcode: $" << std::hex << static_cast<int>(opCode) << "\n";
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
            programCounter = static_cast<unsigned short>((PCH * 0x100) + PCL);
            std::cout << std::hex << "PC: " << static_cast<int>(programCounter) << "\n";

            run();
        }

        void test1()
        {
            reset();
            std::cout << "A register: " << std::hex << static_cast<int>(A) << "\n";
            std::cout << "X register: " << std::hex << static_cast<int>(X) << "\n";
            std::cout << "Y register: " << std::hex << static_cast<int>(Y) << "\n";
        }

        void test2()
        {
            reset();
            std::cout << "X register: " << std::hex << static_cast<int>(X) << "\n";
            std::cout << "Y register: " << std::hex << static_cast<int>(Y) << "\n";
            std::cout << "Address 0x0000 (should read $5a): " << std::hex << static_cast<int>(read(0x0)) << "\n";
            std::cout << "Address 0x0550 (should read $80): " << std::hex << static_cast<int>(read(0x0)) << "\n";
            std::cout << "A register (should read 5a): " << std::hex << static_cast<int>(A) << "\n";
            std::cout << "Address 0x0001 (should read 5a): " << std::hex << static_cast<int>(read(0x1)) << "\n";
            std::cout << "Address 0x0002 (should read 5a): " << std::hex << static_cast<int>(read(0x2)) << "\n";
        }
        void test3()
        {
            reset();
            std::cout << "Address 0x0000 (should read $01) " << std::hex << static_cast<int>(read(0x0)) << "\n";
            std::cout << "Final PC Location (should be $8017) " << std::hex << static_cast<int>(programCounter) << "\n";
        }

};

