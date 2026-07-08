#include <iostream>
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

typedef uint8_t byte;
class Nesmulator
{
    private:
        unsigned short programCounter;
        byte A; // Math & Bitwise operations
        byte X; // Indexing & Counting
        byte Y; // Indexing & Counting

        std::vector<byte> RAM; // Address $0000 to $07FF
        std::vector<byte> ROM; // Address $8000 to $FFFF
        std::array<byte, 0x10> header;

        std::ifstream romFile;

    public:
        Nesmulator(std::string romFile)
        {
            this->romFile.open(romFile, std::ios::in | std::ios::binary);
            if (!this->romFile.is_open()) std::cout << "Failed to open ROM file\n";

            RAM.resize(0x0800);
            ROM.resize(0x8000);
        }
        byte read(unsigned short address)
        {
            if (address < 0x0800) return RAM[address];
            if (address >= 0x8000) return ROM[address - 0x8000];
        }
        void reset()
        {
            // We read the romfile into an array, throw the iNES header in another array, then 
            // throw the rest of the romfile into ROM
            std::vector<byte> headeredRom;
            headeredRom.resize(0x8000);
            romFile.read(reinterpret_cast<char*>(headeredRom.data()), 0x8000);
            std::copy(headeredRom.begin(), headeredRom.begin() + 0x10, header.begin());
            std::copy(headeredRom.begin() + 0x10, headeredRom.end(), ROM.begin());
            
            // find the freakign PC (last 6 bytes of the ROM)
            byte PCL = read(0xFFFC); // PC low byte
            byte PCH = read(0xFFFD); // PC high byte
            // Since PC is in little endian, shift the high byte two decimal places to the left, then add the low byte. Neat!
            programCounter = static_cast<unsigned short>((PCH * 0x100) + PCL);
        }
};

int main()
{

}
