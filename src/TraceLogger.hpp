#include <cstdint>
#include <array>
#include <ios>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

typedef uint8_t byte;
typedef uint16_t word;

class TraceLogger
{
    public:
        TraceLogger()
        {
            
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

        std::string logLine(word programCounter, byte opCode, byte A, byte X, byte Y, byte SP)
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
            logLine << "SP: " << hex8(SP) << "    ";
            return logLine.str();
        }
};
