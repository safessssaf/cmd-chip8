#include <algorithm>
#include <array>
#include <iostream>
#include <cstdint>
#include <fstream>
#include <random>
#include <ratio>
#include <sstream>
#include <synchapi.h>
#include <thread>
#include <utilapiset.h>
#include <windows.h>
#include <chrono>
#include <winsock.h>

using namespace std;
using namespace std::chrono; 

std::ofstream log_file("debug.txt");
int screen_width = 64;
int screen_height = 32;
char color_pallet[] = {'a', 'b', 'c', '@', '0','u'};
int current_pallet = 33;
struct chip8
{
  bool incerment_PC = true;
  bool keyboard[16] = {};
  uint16_t instuction = 0;
  uint16_t stack[16] = {};
  uint8_t SP = 0;
  uint16_t PC = 0x200;
  std::array<uint8_t, 4096> memory = {};
  std::array<uint8_t, 16> registers = {};
  uint16_t register_i = 0;
  uint8_t timer = 0;
  uint8_t sound_timer = 0;
  wchar_t *screen = new wchar_t [screen_height*screen_width]();
  uint8_t characters[0x50] =
  {
      // 0
      0xF0, 0x90, 0x90, 0x90, 0xF0,

      // 1
      0x20, 0x60, 0x20, 0x20, 0x70,

      // 2
      0xF0, 0x10, 0xF0, 0x80, 0xF0,

      // 3
      0xF0, 0x10, 0xF0, 0x10, 0xF0,

      // 4
      0x90, 0x90, 0xF0, 0x10, 0x10,

      // 5
      0xF0, 0x80, 0xF0, 0x10, 0xF0,

      // 6
      0xF0, 0x80, 0xF0, 0x90, 0xF0,

      // 7
      0xF0, 0x10, 0x20, 0x40, 0x40,

      // 8
      0xF0, 0x90, 0xF0, 0x90, 0xF0,

      // 9
      0xF0, 0x90, 0xF0, 0x10, 0xF0,

      // A
      0xF0, 0x90, 0xF0, 0x90, 0x90,

      // B
      0xE0, 0x90, 0xE0, 0x90, 0xE0,

      // C
      0xF0, 0x80, 0x80, 0x80, 0xF0,

      // D
      0xE0, 0x90, 0x90, 0x90, 0xE0,
  
      // E
      0xF0, 0x80, 0xF0, 0x80, 0xF0,

      // F
      0xF0, 0x80, 0xF0, 0x80, 0x80
  };
};
u_int keyboard_assigment(uint8_t vx)
{
  unsigned short keymaping[16]= {
    '1','2','3','4'
    ,'Q','W','E','R'
    ,'A','S','D','F'
    ,'Z','X','C','V'
  };
  return  (GetKeyState(keymaping[vx]) & 0x8000) != 0;
}
int random_int(int min, int max)
{
    static random_device rd;          // entropy source
    static mt19937 gen(rd());          // Mersenne Twister engine
    uniform_int_distribution<int> dist(min, max);

    return dist(gen);
}
chip8 loading_rom(chip8 &memory_chip, string file_path)
{
    fstream file;
    file.open(file_path, ios::binary | ios::in);

    if(file.is_open())
    {       
                  
      file.seekg(0, ios::end);
      streamsize size = file.tellg();
      file.seekg(0, ios::beg);
      file.read(reinterpret_cast<char *>((memory_chip.memory.data()) + 0x200), size);

    }
    return memory_chip;
}


void CLS(chip8 &chip)
{
  for(int i = 0 ;  i < screen_height * screen_width ; i++)
  {
    chip.screen[i] = ' '; 
  }
}
void RET(chip8 &chip)
{
  chip.incerment_PC = false;
  chip.PC = chip.stack[chip.SP];
  --chip.SP;
}

void JP(chip8 &chip, uint16_t current_instruction)
{  
  chip.incerment_PC = false;
  uint16_t jump_address = (current_instruction & 0x0FFF);
  chip.PC = jump_address;
}
void call_addr(chip8 &chip, uint16_t current_instruction)
{
  chip.incerment_PC = false;
  uint16_t jump_address = (current_instruction & 0x0FFF);
  chip.SP ++;
  chip.stack[chip.SP] = chip.PC + 2;
  chip.PC = jump_address;
}
void SE(chip8 &chip, uint16_t current_instruction)
{
  chip.incerment_PC = false;
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00FF);
  chip.PC += (chip.registers[reg_current] == cmp_value)? 4:2;
}
void SNE(chip8 &chip, uint16_t current_instruction)
{
  chip.incerment_PC = false;
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00FF);
  chip.PC += (chip.registers[reg_current] != cmp_value)? 4:2;
}
void SE_REG(chip8 &chip, uint16_t current_instruction)
{

  chip.incerment_PC = false;
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  chip.PC += (chip.registers[reg_current] == chip.registers[cmp_value])? 4:2;
}
void LD(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00FF);
  chip.registers[reg_current] = cmp_value;
 
}
void ADD(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00FF);
  chip.registers[reg_current] += cmp_value;

}
void LD_reg(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  chip.registers[reg_current] = chip.registers[cmp_value];

}
void OR(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  chip.registers[reg_current] |= chip.registers[cmp_value];

}
void AND(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  chip.registers[reg_current] &= chip.registers[cmp_value];

}
void XOR(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  chip.registers[reg_current] ^= chip.registers[cmp_value];

}
void ADD_reg(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  uint16_t added_result = (chip.registers[reg_current] + chip.registers[cmp_value]);
  chip.registers[reg_current] = added_result & 0x00FF;
  chip.registers[0xF] = ((added_result & 0xFF00) != 0x0000)? 1:0;
}
void SUB_reg(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  uint8_t carry = (chip.registers[reg_current] >= chip.registers[cmp_value])? 1 : 0;
  uint16_t added_result = (chip.registers[reg_current] - chip.registers[cmp_value]);
  chip.registers[reg_current] = added_result;
  chip.registers[0xF] = carry; 
}
void SUBN_reg(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  uint16_t added_result = (chip.registers[cmp_value] - chip.registers[reg_current]);
  chip.registers[reg_current] = added_result;
  chip.registers[0xF] = (chip.registers[reg_current] <= chip.registers[cmp_value])? 1 : 0;
}

void SNE_REG(chip8 &chip, uint16_t current_instruction)
{
  chip.incerment_PC = false;
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00F0) >> 4;
  chip.PC += (chip.registers[reg_current] != chip.registers[cmp_value])? 4:2;
}
void SHL(chip8 &chip, uint16_t current_instruction)
{ 
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (chip.registers[reg_current] & 0x80) >> 7;
  chip.registers[reg_current] = chip.registers[reg_current] * 2;
  chip.registers[0xF] = (cmp_value == 1)? 1 : 0;
}

void SHR(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (chip.registers[reg_current] & 0x1);
  chip.registers[reg_current] = chip.registers[reg_current] / 2;
  chip.registers[0xF] = (cmp_value == 1)? 1 : 0;
}
void LD_I(chip8 &chip, uint16_t current_instruction)
{
  uint16_t jump_address = (current_instruction & 0x0FFF);
  chip.register_i = jump_address;
}
void JP_V0(chip8 &chip, uint16_t current_instruction)
{
  chip.incerment_PC = false;
  uint16_t jump_address = (current_instruction & 0x0FFF);
  chip.PC = jump_address + chip.registers[0x0];
}
void RND(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint8_t cmp_value = (current_instruction & 0x00FF);
  uint8_t rand_value= random_int(0, 255);

  chip.registers[reg_current] = cmp_value & rand_value;

}

void SKP(chip8 &chip, uint16_t current_instruction)
{
  chip.incerment_PC = false;
  u_int reg_current =  (current_instruction & 0x0F00) >> 8;
  chip.PC += (keyboard_assigment(chip.registers[reg_current]) == 0x1)? 4:2;
}
void SKNP(chip8 &chip, uint16_t current_instruction)
{
  chip.incerment_PC = false;
  u_int reg_current =  (current_instruction & 0x0F00) >> 8;
  chip.PC += (keyboard_assigment(chip.registers[reg_current]) != 0x1)? 4:2;
}
void REG_TIMER(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8;
  chip.registers[reg_current] = chip.timer;
}
void INPUT_WAIT(chip8 &chip, uint16_t current_instruction)
{
  bool input_pressed = false;

  u_int reg_current =  (current_instruction & 0x0F00) >> 8;
  for(int i = 0; i < 16; i++)
  {
    chip.incerment_PC = false;
    if(keyboard_assigment(i) == true)
    {
      chip.registers[reg_current] = i;
      chip.incerment_PC = true;
      break;
    }
  }
  
}

void SET_TIMER(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8;
  chip.timer = chip.registers[reg_current];

}
void SET_SOUND_TIMER(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8;
  chip.sound_timer = chip.registers[reg_current];

}
void ADD_I(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  uint16_t added_result = (chip.registers[reg_current]) + chip.register_i;
  chip.register_i = added_result;

}

void MEMORY_SETI(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 

  int digit = chip.registers[reg_current] % 10;
  int tenth_digit = ((chip.registers[reg_current]) / 10) % 10;
  int hundred_digit = (chip.registers[reg_current]) / 100;
  chip.memory[chip.register_i] = hundred_digit;
  chip.memory[chip.register_i + 1] = tenth_digit;
  chip.memory[chip.register_i + 2] = digit;

}
void STORE_IST(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  for (int n = 0; n <= reg_current; n++)
  {
    chip.memory[chip.register_i + n] = chip.registers[n];
  }

}
void STORE_IST_memory(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  for (int n = 0; n <= reg_current; n++)
  {
    chip.memory[chip.register_i + n] = chip.registers[n];
  }

}
void STORE_IST_reg(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8; 
  for (int n = 0; n <= reg_current; n++)
  {
    chip.registers[n] = chip.memory[n + chip.register_i];
  }
}
void drw(chip8 &chip, uint16_t current_instruction)
{
  chip.registers[0xf] = 0;
  uint8_t vx = (current_instruction & 0x0F00) >> 8; 
  uint8_t reg_x =  chip.registers[vx];
  uint8_t vy = (current_instruction & 0x00F0) >> 4;
  uint8_t reg_y = chip.registers[vy];
  uint8_t num_bytes = (current_instruction & 0x000F);
  for (int n = 0; n < num_bytes; n++)
  { 
      //log_file << "I: " << std::hex << chip.register_i
        //  << " N: " << std::dec << (int)n << "\n";
    int screen_y = ((reg_y + n) % screen_height);
    for(int i = 0; i < 8; i++)
    {
      if(((chip.memory[chip.register_i + n] >> i) & 0x1) == 0x1)
      {
        int screen_x = (reg_x + (7 - i)) %screen_width;
        if(chip.screen[screen_x + screen_y * screen_width] != ' ')
        {
          chip.screen[screen_x + screen_y * screen_width] = ' ';
          chip.registers[0xf] = 1;
        }else
        {
          chip.screen[screen_x + screen_y * screen_width] = char(current_pallet);
        }
      }
    }
  }
 }
void load_I(chip8 &chip, uint16_t current_instruction)
{
  u_int reg_current =  (current_instruction & 0x0F00) >> 8;
  chip.register_i =  chip.registers[reg_current] * 5;
}
void instruction_manger(uint16_t current_instruction, chip8 &chip)
{
  switch(current_instruction & 0xF000)
  {
    case 0x0000:
      switch(current_instruction & 0x00FF)
      {
        case 0x00E0:
          CLS(chip);
          return;
        case 0x00EE:
          RET(chip);
          return;
      }
    return;

    case 0x1000:
      JP(chip, current_instruction);
      return;
    case 0x2000:
      call_addr(chip, current_instruction);
      return;
    case 0x3000:
      SE(chip, current_instruction);
      return;
    case 0x4000:
      SNE(chip, current_instruction);
      return;
    case 0x5000:
      SE_REG(chip, current_instruction);
      return;
    case 0x6000:
      LD(chip, current_instruction);
      return;

    case 0x7000:
      ADD(chip, current_instruction);
      return;
    case 0x8000:
      switch (current_instruction & 0x000F) 
      {
        case 0x0000:
          LD_reg(chip, current_instruction);
          return;
        case 0x0001:
          OR(chip, current_instruction);
          return;
        case 0x0002:
          AND(chip, current_instruction);
          return;
        case 0x0003:
          XOR(chip, current_instruction);
          return;
        case 0x0004:
          ADD_reg(chip, current_instruction);
          return;
        case 0x0005:
          SUB_reg(chip, current_instruction);
          return;
        case 0x0006:
          SHR(chip, current_instruction);
          return;
        case 0x0007:
          SUBN_reg(chip, current_instruction);
          return;
        case 0x000E:
          SHL(chip, current_instruction);
          return;
      }
      return;
    case 0x9000:
      SNE_REG(chip, current_instruction);
      return;

    case 0xA000:
      LD_I(chip, current_instruction);
      return;
    case 0xB000:
      JP_V0(chip, current_instruction);
      return;
    case 0xC000:
      RND(chip, current_instruction);
      return;

    case 0xD000:
      drw(chip, current_instruction);
      return;
    case 0xE000:
      switch(current_instruction & 0x00FF)
      {
        case 0x009E:
          SKP(chip, current_instruction);
          return;
        case 0xA1:
          SKNP(chip, current_instruction);
          return;
      }
      return;

    case 0xF000:
      switch (current_instruction & 0x00FF) 
      {
        case 0x0007:
          REG_TIMER(chip, current_instruction);
          return;
        case 0x000A:
          INPUT_WAIT(chip, current_instruction);
          return;
        case 0x0015:
          SET_TIMER(chip, current_instruction);
          return;
        case 0x0018:
          SET_SOUND_TIMER(chip, current_instruction);
          return;
        case 0x001E:
          ADD_I(chip, current_instruction);
          return;
        case 0x0029:
          load_I(chip, current_instruction);
          return;
        case 0x0033:
          MEMORY_SETI(chip, current_instruction);
          return;
        case 0x0055:
          STORE_IST_memory(chip, current_instruction);
          return;
        case 0x0065:
          STORE_IST_reg(chip, current_instruction);
          return;
      }
      return;

  }
}

void time_update(auto &start_time, chip8 &chip)
{
  if(chip.timer > 0)
  {
    auto current_time = steady_clock::now();
    while(current_time - start_time >= 16.666666666666667ms)
    {
      chip.timer -=1;
      start_time = steady_clock::now();
    }
  }
}
void sound_time_update(auto &start_time, chip8 &chip)
{
  if(chip.sound_timer > 0)
  {
    auto current_time = steady_clock::now();
    while(current_time - start_time >= 16.666666666666667ms)
    {
      chip.sound_timer -= 1;
      start_time = steady_clock::now();
    }
  }
}
stringstream debug;
void PalletChanger(chip8 &chip)
{
  if(GetAsyncKeyState((unsigned short)'L') & 1)
  {
    if(current_pallet < 126)
    {
      current_pallet++;
    }else
    {
      current_pallet = 33;
    }
    for(int i =  0; i  < screen_width * screen_height; i++)
    {
      if(chip.screen[i] != ' ')
      {
        chip.screen[i] = char(current_pallet);
      }
    }
  }
}
int main(int argc, char *argv[])
{
    HANDLE hBuffer = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL
    );
    chip8 main_chip;
    CLS(main_chip);
    std::copy(main_chip.characters, main_chip.characters + 0x50, main_chip.memory.data());
    loading_rom(main_chip, argv[1]);
    auto start_time = steady_clock::now();
    auto start_sound_time = steady_clock::now();
    main_chip.instuction = (main_chip.memory[main_chip.PC] << 8 | main_chip.memory[main_chip.PC + 1]);
    SetConsoleActiveScreenBuffer(hBuffer);
    DWORD bytesWritten = 0;
    auto begin_tick_update = steady_clock::now();
    while((GetKeyState(VK_ESCAPE) & 0x8000) == false)
    {
      auto current_tick_time = steady_clock::now();
      if(current_tick_time - begin_tick_update >= 1ms)
      {
        //if(GetAsyncKeyState(VK_UP) & 1)
        //{
            PalletChanger(main_chip);
            debug <<hex << " PC: "<< (int)main_chip.PC<< ", previous instruction: "<< (int)(main_chip.instuction);
            debug <<", v0: "<< (int)main_chip.registers[0x0]<<", v1: "<< (int)main_chip.registers[0x1]<<", v2: "<< (int)main_chip.registers[0x2] <<", v3: "<< (int)main_chip.registers[0x3]<<
            ", v4: "<< (int)main_chip.registers[0x4]<<", v5: "<< (int)main_chip.registers[0x5]<<", v6: "<< (int)main_chip.registers[0x6]<<", v7: "<< (int)main_chip.registers[0x7]
            <<", v8: "<< (int)main_chip.registers[0x8]<<", v9: "<< (int)main_chip.registers[0x9]<<", va: "<< (int)main_chip.registers[0xa]<<", vb: "<< (int)main_chip.registers[0xb]<<", vc:        "<< (int)main_chip.registers[0xc]
            <<", vd: "<< (int)main_chip.registers[0xd]<<
            ", ve: "<< (int)main_chip.registers[0xe]<<", vf: "<< (int)main_chip.registers[0xf]<<", I:"<<(int)main_chip.register_i <<"\n";

            WriteConsoleOutputCharacterW(hBuffer, main_chip.screen, screen_width * screen_height, {0,0}, &bytesWritten);
            instruction_manger(main_chip.instuction, main_chip);
            sound_time_update(start_sound_time, main_chip);
            time_update(start_time, main_chip);
            main_chip.PC += (main_chip.incerment_PC)? 2:0;
            main_chip.instuction = (main_chip.memory[main_chip.PC] << 8 | main_chip.memory[main_chip.PC + 1]);
            main_chip.incerment_PC = true;
            OutputDebugStringA(debug.str().c_str());
            debug.str("");
       // }
        begin_tick_update = steady_clock::now();
      }
    }
}
