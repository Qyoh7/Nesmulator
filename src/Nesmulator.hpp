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
        unsigned short programCounter; 
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

            flagCarry = false;
            flagZero = false;
            flagInterruptDisable = true;
            flagDecimal = false;
            flagOverflow = false;
            flagNegative = false;
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

        void run()
        {
            while (!cpuHalted)
            {
                emulateCpu();
            }
        }

        void emulateCpu()
        {
            byte opCode = read(programCounter);
            byte temp;
            byte tempLow;
            byte tempHigh;
            programCounter++;
            switch (opCode)
            {
                case 0x02: // HLT
                    cpuHalted = true;
                    programCounter++;
                    break;

                case 0x84: // STY Zero Page
                    temp = read(programCounter);
                    programCounter++;
                    write(temp, Y);
                    cycles = 3;
                    break;

                case 0x85: // STA Zero Page
                    temp = read(programCounter);
                    programCounter++;
                    write(temp, A);
                    cycles = 3;
                    break;

                case 0x86: // STX Zero Page
                    temp = read(programCounter);
                    programCounter++;
                    write(temp, X);
                    cycles = 3;
                    break;

                case 0x8C: // STY Absolute
                    tempLow = read(programCounter);
                    programCounter++;
                    tempHigh = read(programCounter);
                    programCounter++;
                    write(static_cast<unsigned short>((tempHigh * 0x100) + tempLow), Y);
                    cycles = 4;
                    break;

                case 0x8D: // STA Absolute
                    tempLow = read(programCounter);
                    programCounter++;
                    tempHigh = read(programCounter);
                    programCounter++;
                    write(static_cast<unsigned short>((tempHigh * 0x100) + tempLow), A);
                    cycles = 4;
                    break;

                case 0x8E: // STX Absolute
                    tempLow = read(programCounter);
                    programCounter++;
                    tempHigh = read(programCounter);
                    programCounter++;
                    write(static_cast<unsigned short>((tempHigh * 0x100) + tempLow), X);
                    cycles = 4;
                    break;

                case 0xA0: // LDY Immediate
                    Y = read(programCounter);
                    programCounter++;
                    flagZero = (Y == 0);
                    flagNegative = (Y > 127);
                    cycles = 2;
                    break;

                case 0xA2: // LDX Immediate
                    X = read(programCounter);
                    programCounter++;
                    flagZero = (X == 0);
                    flagNegative = (X > 127);
                    cycles = 2;
                    break;

                case 0xA5: // LDA Zero Page
                    temp = read(programCounter);
                    programCounter++;
                    A = read(temp);
                    flagZero = (A == 0);
                    flagNegative = (A > 127);
                    cycles = 3;
                    break;

                case 0xA9: // LDA Immediate
                    A = read(programCounter);
                    programCounter++;
                    flagZero = (A == 0);
                    flagNegative = (A > 127);
                    cycles = 2;
                    break;

                case 0xAD: // LDA Absolute
                    tempLow = read(programCounter);
                    programCounter++;
                    tempHigh = read(programCounter);
                    programCounter++;
                    A = read(static_cast<unsigned short>((tempHigh * 0x100) + tempLow));
                    flagZero = (A == 0);
                    flagNegative = (A > 127);
                    break;

                default:
                    std::cerr << "Invalid opcode: $" << std::hex << static_cast<int>(opCode) << "\n";
                    break;
            }
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
            else 
            {
                std::cerr << "Unable to write, Address " << address << " Is out of bounds!\n";
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
};

