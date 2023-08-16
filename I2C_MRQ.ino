/*
   Scenic 2 Tuner List radio dash prototype

   Inspired by http://monorailc.at/cms/index.php?d=2018/03/17/19/20/30-autoradio-tuner-list-4-ecran-lcd

*/
#define I2C_7BITADDR 0x23 // Tuner List dashboard

// try to mitigate protocol errors by restating init
int max_errors = -1; //-1 = disable
int errors = 0; // read error counter

boolean powered = false; //is the dash powerd?
boolean mrq_forced = false;

int counter = 0;
int display_cycle = 0;

/*
    Dash data packets
*/
char i2cBuffer[16];
static byte INIT[1] = {0x10};
static byte PONG[1] = {0x11};

#define PWR_PIN 3 // nano D5 power the dash (Low level 5V relay)

//radio pin
#define SEND_PIN 4 //to SRC opto

#define I2C_TIMEOUT 1000
#define SDA_PORT PORTC
#define SDA_PIN 4 // = A4
#define SCL_PORT PORTC
#define SCL_PIN 5 // = A5
#define MRQ_PIN 2 // = D2

//https://registry.platformio.org/libraries/felias-fogg/SoftI2CMaster
#include <SoftI2CMaster.h>

//https://electropeak.com/learn/interfacing-x9c104-100k-ohm-digital-potentiometer-module-with-arduino/
#include <DigiPotX9Cxxx.h>
DigiPot pot(7, 6, 5);

// Pull MRQ line to transmit data
void PULL_MRQ()
{
  //wait if MRQ pin is low (reading from dash)
  while (!digitalRead(MRQ_PIN));

  // stop listening for MRQ events
  detachInterrupt(digitalPinToInterrupt(MRQ_PIN));

  mrq_forced = true;

  pinMode(MRQ_PIN, OUTPUT);
  digitalWrite(MRQ_PIN, LOW);
}

// release MRQ line to listen to data
void RELEASE_MRQ()
{
  mrq_forced = false;

  pinMode(MRQ_PIN, INPUT);

  // start listening for MRQ events
  attachInterrupt(digitalPinToInterrupt(MRQ_PIN), MRQ_pulled, LOW);
}

/*
   Read from BUS
*/
void read()
{
  uint8_t x, num_bytes;
  i2c_rep_start((I2C_7BITADDR << 1) | I2C_READ);
  //read length
  num_bytes = i2c_read(false);
  //Serial.print("READ(" + (String)num_bytes + "):");
  if (num_bytes > 0)
  {
    //error in read size, appear
    if (num_bytes > sizeof(i2cBuffer))
    {
      num_bytes = sizeof(i2cBuffer);
      errors++;
      //Serial.println("I2C read error " + (String)num_bytes);
      //i2c_stop();
      //return;
    }
    //read data
    for (x = 0; x < num_bytes; x++)
    {
      byte value = i2c_read(false);
      i2cBuffer[x] = value;
      //Serial.print(value, HEX);
    }
  }
  //Serial.println();
  i2c_stop();
}

/*
   Write to BUS
   You must set the length of the data
*/
void write(byte data[], uint8_t bytes)
{
  PULL_MRQ();
  i2c_start((I2C_7BITADDR << 1) | I2C_WRITE);
  //Serial.print("WRITE(" + (String)bytes + "):");

  //write length
  i2c_write(bytes);
  //write data
  for (byte i = 0; i < bytes; i++)
  {
    i2c_write(data[i]);
    //Serial.print(data[i], HEX);
  }
  //reset data
  data = {};
  //Serial.println();
  i2c_stop();
  RELEASE_MRQ();
}

// Power ON or OFF the dashboard
// State : true = power on, false = power off
void power(boolean state)
{
  if (state)//radio ON relay control
  {
    digitalWrite(PWR_PIN, HIGH);
    delay(500);
  }
  if (state) //DO power on init
  {
    errors = 0;
    while (!powered) //init sequence
    {
      //wait 0x47+, expect 0x01+ 0x00+
      read();
      //Serial.println(read1, DEC);
      //send 0x46+ 0x01+ 0x10+
      write(INIT, sizeof(INIT));

      //wait 0x47+, expect 0x01+ 0x00+
      read();
      //Serial.println(read2, DEC);
      //send 0x46+ 0x01+ 0x11+
      write(PONG, sizeof(PONG));

      //init done !
      Serial.println("Scenic dash INIT DONE !");
      powered = state;
      delay(50);
    }
  }
  if (!state) {
    //radio OFF relay control
    digitalWrite(PWR_PIN, LOW);
    powered = state;
  }
}

//display message max 8 chars
//TODO add 8+ chars messages
void displayString(String message)
{
  // LCD display message
  byte data[15] = {
    0x90, // ?
    0x7F, // ?
    0x55, // 4E=AF+inews blink, 4F="AF", 50=itrafic fixed+inews fixed, 52=itrafic fixed+inews blink, 53=itrafic fixed, 54=inews fixed, 55=none, 56=inews blink
    0xFF, // ?
    0xFF, // tuner mode: 00=preset+manu, DF=manu, FF=none
    0x60, // channels pressets: 60=none, 61=1 //TODO
    0x01, // text scroll: 00/01=none, 02=left
    // 8 hex chars string
    //M   space space   6     8     4   space space
    0x4D, 0x20, 0x20, 0x36, 0x38, 0x34, 0x20, 0x20
  };

  //replace chars in display message packet
  byte messageSize = sizeof(message);
  int letter = 0;
  for (int i = 7; i < 15; i++)
  {
    if (i > messageSize)
      data[i] = message[letter];
    else
      data[i] = 0x20; // spaces
    letter++;
  }
  write(data, sizeof(data));
}

void getButtons()
{
  //buttons
  if ((i2cBuffer[0] & 0x82) && (i2cBuffer[1] & 0x91))
  {
    switch (i2cBuffer[2])
    {
      case 0:
        switch (i2cBuffer[3])
        {
          case 0:
            //Serial.println("WHEEL pressed !");
            break;
          case 1:
            Serial.println("SRC - !");
            displayString("Band");
            RADIO_Button(62000, 50); // band
            break;
          case 2:
            Serial.println("SRC +");
            displayString("Source");
            RADIO_Button(1200, 50); // src
            break;
          case 3:
            Serial.println("VOL + !");
            displayString("VOL +");
            RADIO_Button(24000, 50);//vol +
            break;
          case 4:
            Serial.println("VOL - !");
            displayString("VOL -");
            RADIO_Button(30000, 50);//vol -
            break;
          case 5:
            Serial.println("MUTE !");
            displayString("MUTE");
            RADIO_Button(3500, 50); //mute
            break;
          case 67:
            Serial.println("VOL + hold !");
            break;
          case 68:
            Serial.println("VOL - hold !");
            break;
          case -123:
            Serial.println("MUTE hold !");
            break;
          case -126:
            Serial.println("SRC + hold !");
            break;
          case -127:
            Serial.println("SRC - hold!");
            break;
          case -128:
            Serial.println("WHEEL hold !");
            break;
        }
        break;
      case 1:
        switch (i2cBuffer[3])
        {
          case 1:
            Serial.println("WHEEL down !");
            displayString("Previous");
            RADIO_Button(10000, 50); //preset-/previous
            break;
          case 65:
            Serial.println("WHEEL up !");
            displayString("Next");
            RADIO_Button(16000, 50); //preset+/next
            break;
        }
        break;
    }
  }
}

/*
    Print the read buffer
*/
void debugRead()
{
  Serial.print("READ(");
  Serial.print(strlen(i2cBuffer));
  Serial.print("):");

  for (int x = 0; x < strlen(i2cBuffer); x++)
  {
    Serial.print(i2cBuffer[x], HEX);
    Serial.print(" ");
  }
  Serial.println("");
}

/*
    Reset the read buffer (0 fill)
*/
void resetReadBuffer()
{
  memset(i2cBuffer, 0, sizeof(i2cBuffer));
}

/*
    Called on MRQ pulled low by dash (mostly on button press)
*/
void MRQ_pulled()
{
  if (powered && !mrq_forced)
  {
    //Serial.println("Message from dash !!!!!!");
    read();

    getButtons();
    //debugRead();
  }
}

void RADIO_Button(long resistance, uint8_t duration)
{
  //100k digipot
  long digipot_max = 100000;
  uint8_t digipot_divisions = 99;

  //digital to resistance - 100000 / 100 * pot_val = Ohm
  //long actual_resistance = (digipot_max / digipot_divisions) * pot.get();

  //resistance to digital - Ohm / 100000 / 100 = pot_val
  uint8_t target_pot = resistance / (digipot_max / digipot_divisions);

  //digipot diff
  int diff_pot = target_pot - pot.get();

  //resistance diff
  //long diff_resistance = resistance - actual_resistance;

  /*Serial.print("ACTUAL POT: ");
    Serial.println(pot.get());
    Serial.print("ACTUAL RES: ");
    Serial.println(actual_resistance);
    Serial.print("TRIGGER RES: ");
    Serial.println(resistance);
    Serial.print("DIFF RES: ");
    Serial.println(diff_resistance);
    Serial.print("TARGET POT: ");
    Serial.println(target_pot);
    Serial.print("DIFF POT: ");
    Serial.println(diff_pot);*/

  if (diff_pot > 0)
  {
    //    Serial.print("increase ");
    //    Serial.println(diff_pot);
    pot.increase(diff_pot);
  }
  if (diff_pot < 0)
  {
    //    Serial.print("decrease ");
    //    Serial.println(diff_pot);
    pot.decrease(-diff_pot);
  }

  //connect the resistance via optocopler fox x ms
  digitalWrite(SEND_PIN, HIGH);
  delay(duration);
  digitalWrite(SEND_PIN, LOW);
}

void setup()
{
  if (!i2c_init())
    Serial.println("I2C init failed");

  Serial.begin(115200);  // start serial for output
  pinMode(PWR_PIN, OUTPUT); //dash power
  digitalWrite(PWR_PIN, LOW);

  pinMode(SEND_PIN, OUTPUT); //
  digitalWrite(SEND_PIN, LOW);

  pot.reset();
  power(true);
}

void loop()
{

  //stop if too much errors
  if (max_errors != -1 && errors > max_errors)
  {
    if (powered) {
      Serial.println("Too much errors, restarting");
      power(false);
      delay(1000);
      power(true);
    }
    return;
  }

  //read the buffer
  getButtons();

  //every 500ms 50*10 ms
  if ((counter % 10) == 0)
  {
    //write(PONG, sizeof(PONG));
    switch (i2cBuffer[0])
    {
      //keepalive
      case 0:
      case 1:
      case 2:
        write(PONG, sizeof(PONG));
        break;
    }
  }

  //every 2000ms 50*40 ms
  if ((counter % 40) == 0)
  {
    //display cycle
    switch (display_cycle)
    {
      case 0:
        displayString("Welcome");
        break;
      case 1:
        displayString("");
        break;
      default:
        display_cycle = 0;
        break;
    }
    display_cycle++;
  }

  //increment counter
  counter++;

  //reste buffer
  resetReadBuffer();

  delay(50);
  //Serial.println("End of loop");
}
