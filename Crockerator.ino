#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

const char upButton = 8;
const char downButton = 12;
const char leftButton = 10;
const char rightButton = 9;
const char confirmButton = 11;
const char bigHeat = A0;
const char smaHeat = A1;
const char cooler = A2;
const char therm = 7;
const char knobCool = A5;
const char knobWarm = A4;
const char knobProgram = A3;
const char knobHot = 13;

const short waitTime = 400;

//The time increments.  In the final product this will be 3600 and 60 to correspond to hours and minutes, but for now they are a second or two because I don't want to wait a long time to test my timer.
const long biginc = 2;
const long smallinc = 1;

//I know these are going to change.  These are various constants for regulating the heating and cooling system.
const int coolerMaxTemp = 150; //maximum value from the thermister the cooling system is allowed to run at.
const int maxTemp = 220;
const int minTemp = 32;
const int coolSystemMax = 40; //maximum value the cooling system is allowed to run at.
const int heatSystemMin = 200;
//note do to problems with the design of the hardware, all thermistor related code is disabled and untested.

// Constants for display.  This is a modfication of the code from Adafruit.
#define OLED_MOSI  2
#define OLED_CLK   3
#define OLED_DC    4
#define OLED_CS    6
#define OLED_RESET 5


Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
#if (SSD1306_LCDHEIGHT != 64)//just a safety measure copied from the example code
#error "Height incorrect, please fix Adafruit_SSD1306.h!";
#endif


//useful constants for the graphics
const int boxwidth = display.width() / 2;
const int modeBoxWidth = display.width() / 4;
const int boxheight = display.height() / 2;
const int modeBoxHeight = display.height() / 2;
const int topHeight = display.height() / 4;
const char modeNames[] = {' ', 'W', 'H', 'C', 'K'};

//0 for off, 1 for warm, 2 for hot, 3 for cold, 4 for program.  Bookkeeping
char mode = 0;


//0 for off, 1 for on.  Just for bookkeeping
char bigHeatMode = 0;
char litHeatMode = 0;
char coolMode = 0;
char programMode = 0;

//for the program mode to communicate between the setup and running functions without parameters.  It can't have parameters because of the spec to run on the queue
int modes[4];
long times[4];
char pos = 0;
char progenable = 0;
long waitProgramTime = 0;
long waitProgramMode = 0;

//to set the program mode
int posposition;
char modeposition = 0;
//to help view what the modes are if the user presses up or down on the button pad whiles setting program elements.
int up = 3, down = 1;
t
//Queue management data.
typedef void (*exec)(void);//this is to allow for the program to manage the queue elements as a type.
exec queue[32];
exec knobCheck;
exec confirmModes;
int queueRead = 0, queueWrite = 0;

void putOnQueue(exec in)
{
  queue[queueWrite] = in;
  if (queueWrite == 31)
    queueWrite = 0;
  else
    ++queueWrite;
}

//This is kept for reference on how to program a waiting function.
void sendBigHeat()
{
  static unsigned long ending = 0;
  static char isOn = 0;
  if (isOn)
  {
    if (millis() > ending)
    {
      isOn = 0;
      digitalWrite(bigHeat, LOW);
    }
    else
      putOnQueue(&sendBigHeat);
  }
  else
  {
    digitalWrite(bigHeat, HIGH);
    ending = millis() + 100;
    isOn = 1;
    putOnQueue(&sendBigHeat);
  }
}

void hotReg()
{ /*
    if(mode==2)
    {
     int temp = analogRead(therm);
     if(temp<heatSystemMin)
       digitalWrite(smaHeat,HIGH);
     else if(temp>maxTemp)
       digitalWrite(smaHeat,LOW);
       putOnQueue(&hotReg);
    }*/
}

void coolReg()
{ /*
    if (mode == 3)
    {
     int temp = analogRead(therm);
     if (temp > coolSystemMax)
       digitalWrite(cooler, HIGH);
     else if (temp < minTemp)
       digitalWrite(cooler, LOW);
     putOnQueue(&coolReg);
    }*/
}

//for when and if I get I figure out how to make the system more efficient if I know a heat is coming up.
void coolRegBeforeHeat()
{

}

void waitOnCool()
{
  digitalWrite(cooler, HIGH);
  /*
  if (analogRead(therm) < coolerMaxTemp)
  {
    digitalWrite(cooler, HIGH);
    putOnQueue(&coolReg);
  }
  else
    putOnQueue(&waitOnCool);
    */
}

void runProgram()
{
  static long until = 0;
  if (!progenable)
  {
    Serial.println("hello");
    return;
  }
  if (until < millis())
  {
    Serial.println(times[pos]);
    if (pos < 4)
    {
      display.setCursor(60, 0);
      display.setTextSize(2);
      mode = modes[pos];
      if (mode == 1)
        display.print("Warm");
      if (mode == 1 || mode == 2)
        digitalWrite(bigHeat, HIGH);
      else
        digitalWrite(bigHeat, LOW);
      if (mode == 2)
      {
        digitalWrite(smaHeat, HIGH);
        display.print("Hot");
      }
      else
        digitalWrite(smaHeat, LOW);
      if (mode == 3)
      {
        display.print("Cool");
        putOnQueue(&waitOnCool);
      }
      else
        digitalWrite(cooler, LOW);
      if (mode == 0)
      {
        until = 0;
        digitalWrite(bigHeat, LOW);
        digitalWrite(smaHeat, LOW);
        digitalWrite(cooler, LOW);
        progenable = 0;
        Serial.println("finished program by running");
        putOnQueue(knobCheck);
        return;
      }
      display.setTextSize(3);
      until = millis() + times[pos];
      ++pos;
    } else
    {
      until = 0;
      mode = 0;
      digitalWrite(bigHeat, LOW);
      digitalWrite(smaHeat, LOW);
      digitalWrite(cooler, LOW);
      progenable = 0;
      putOnQueue(knobCheck);
      Serial.println("finished program");
      return;
    }
  }
  else
  {
    static long lastsmall = 0;
    long untilEnd = (until - millis()) / 1000;
    long bigs = untilEnd / biginc;
    long smalls = (untilEnd - biginc * bigs) / smallinc;
    if (lastsmall != smalls)
    {
      //the plus two's are to accomodate the strip around the box.
      display.setCursor(2, topHeight + 2);
      display.print(bigs);
      display.setCursor(boxwidth + 2, topHeight + 2);
      display.print(smalls);
      display.display();
    }
    lastsmall = smalls;
  }
  if (mode != 0)
    putOnQueue(&runProgram);
}

void drawModeBoxes()
{
  const int width = display.width() / 4;
  display.clearDisplay();
  display.setTextColor(WHITE);
  for (int a = 0; a < posposition; ++a)
  {
    display.fillRect(a * width, topHeight, width, modeBoxHeight, BLACK);
    display.drawRect(a * width, topHeight, width, modeBoxHeight, WHITE);
    display.setCursor(a * width, topHeight);
    display.print(modeNames[modes[a]]);
  }

  for (int a = posposition + 1; a < 4; ++a)
  {
    display.fillRect(a * width, topHeight, width, modeBoxHeight, BLACK);
    display.drawRect(a * width, topHeight, width, modeBoxHeight, WHITE);
    display.setCursor(a * width, topHeight);
    display.print(modeNames[modes[a]]);
  }
  display.setTextColor(BLACK);
  display.fillRect(posposition * width, topHeight, width, modeBoxHeight, WHITE);
  display.setCursor(posposition * width, topHeight);
  display.print(modeNames[modes[posposition]]);
}

void setProgramModes();

void setProgramTimes()
{
  const long smallinbig = biginc / smallinc;
  static long toTime = 0;
  static long increment = 2;
  static long smalls = 0;
  static long bigs = 0;
  if (mode != 4)
    return;
  if (millis() > waitProgramTime)
  {
    if (increment == smallinc && digitalRead(leftButton) == LOW)
    {
      increment = biginc;
      //it would be really nice if there was an inversion function in a box but there isn't.
      display.fillRect(0, topHeight, boxwidth, boxheight, WHITE);
      display.setTextColor(BLACK);
      display.setCursor( 0, topHeight);
      display.print(bigs);
      display.fillRect(boxwidth, topHeight, boxwidth, boxheight, BLACK);
      display.drawRect(boxwidth, topHeight, boxwidth, boxheight, WHITE);
      display.setTextColor(WHITE);
      display.setCursor(boxwidth, topHeight);
      display.print(smalls);
      Serial.println(bigs);
      Serial.println(smalls);
      display.display();
      waitProgramTime = millis() + waitTime;
    }
    if (increment == biginc && digitalRead(rightButton) == LOW)
    {
      increment = smallinc;
      display.fillRect(0, topHeight, boxwidth, boxheight, BLACK);
      display.drawRect(0, topHeight, boxwidth, boxheight, WHITE);
      display.setTextColor(WHITE);
      display.setCursor(0, topHeight);
      display.print(bigs);
      display.fillRect(boxwidth, topHeight, boxwidth, boxheight, WHITE);
      display.setTextColor(BLACK);
      display.setCursor(boxwidth, topHeight);
      display.print(smalls);
      display.display();
      waitProgramTime = millis() + waitTime;
    }
    if (digitalRead(upButton) == LOW)
    {
      if (increment == smallinc)
      {
        if (smalls == 0)
        {
          if (bigs != 0)
          {
            --bigs;
            smalls = smallinbig - 1;
            display.setTextColor(WHITE, BLACK);
            display.setCursor(0, topHeight);
            display.print(bigs);
          }
        } else
          --smalls;
        display.setTextColor(BLACK, WHITE);
        display.setCursor(boxwidth, topHeight);
        display.print(smalls);
      } else if (bigs != 0)
      {
        --bigs;
        display.setTextColor(BLACK, WHITE);
        display.setCursor(0, topHeight);
        display.print(bigs);
      }
      display.display();
      waitProgramTime = millis() + waitTime;
    }
    if (digitalRead(downButton) == LOW)
    {
      if (increment == smallinc)
      {
        if (smalls == smallinbig - 1)
        {
          smalls = 0;
          ++bigs;
          display.setTextColor(WHITE, BLACK);
          display.setCursor(0, topHeight);
          display.print(bigs);
        } else
          ++smalls;
        display.setTextColor(BLACK, WHITE);
        display.setCursor(boxwidth, topHeight);
        display.print(smalls);
      } else
      {
        ++bigs;
        display.setTextColor(BLACK, WHITE);
        display.setCursor(0, topHeight);
        display.print(bigs);
      }
      waitProgramTime = millis() + waitTime;
      display.display();
    }
    if (digitalRead(confirmButton) == LOW)
    {
      times[posposition] = (bigs * biginc + smalls * smallinc) * 1000;
      bigs = 0;
      smalls = 0;
      drawModeBoxes();
      increment = biginc;
      display.display();
      putOnQueue(&setProgramModes);
      waitProgramMode = millis() + waitTime;
      Serial.println("return");
      return;
    }
  }
  putOnQueue(&setProgramTimes);
}

void drawYes()
{
  display.fillRect(boxwidth, topHeight, boxwidth, boxheight, BLACK);
  display.drawRect(boxwidth, topHeight, boxwidth, boxheight, WHITE);
  display.setTextColor(WHITE);
  display.setCursor(boxwidth, topHeight);
  display.print("NO");
  display.fillRect(0, topHeight, boxwidth, boxheight, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(0, topHeight);
  display.print("YES");
  display.display();
}

void drawNewSelect(int newposition, int oldposition)
{
  const int width = display.width() / 4;
  Serial.print(newposition);
  Serial.println(oldposition);
  display.fillRect(oldposition * width, 0, width, display.height(), BLACK);
  display.drawRect(oldposition * width, topHeight, width, modeBoxHeight, WHITE);
  display.fillRect(newposition * width, topHeight, width, modeBoxHeight, WHITE);

  display.setTextColor(WHITE);

  display.setCursor(oldposition * width, topHeight);
  display.print(modeNames[modes[oldposition]]);

  display.setTextColor(BLACK);
  display.setCursor(newposition * width, topHeight);
  display.print(modeNames[modes[newposition]]);

  display.display();
}

void drawNewMode()
{
  const int fontwidth = 10;
  const int offset = modeBoxWidth / 2 - fontwidth / 2; //to make it centered above the box.
  char upi = modeNames[up], midi = modeNames[modes[posposition]], downi = modeNames[down]; // Putting them in variables for some reason fixes a bug where it would generate garbage on the screen.
  Serial.print(upi);
  Serial.print(midi);
  Serial.println(downi);
  display.setTextColor(WHITE, BLACK);
  display.setTextSize(2);
  display.setCursor(posposition * modeBoxWidth + offset, 0);
  display.print(upi);
  display.setCursor(posposition * modeBoxWidth + offset, topHeight + boxheight);
  display.print(downi);
  display.setTextColor(BLACK, WHITE);
  display.setTextSize(4);
  display.setCursor(posposition * modeBoxWidth, topHeight);
  display.print(midi);
  display.display();
}

void doNotConfirmProgramModes()
{
  if (waitProgramMode < millis())
  {
    if (digitalRead(leftButton) == LOW)
    {
      putOnQueue(confirmModes);
      drawYes();
      waitProgramMode = waitTime + millis();
      return;
    }
    if (digitalRead(confirmButton) == LOW)
    {
      putOnQueue(&setProgramModes);
      drawModeBoxes();
      display.display();
      waitProgramMode = waitTime + millis();
      return;
    }
  }
  putOnQueue(&doNotConfirmProgramModes);
}

void confirmProgramModes()
{
  if (waitProgramMode < millis())
  {
    if (digitalRead(rightButton) == LOW)
    {

      display.fillRect(boxwidth, topHeight, boxwidth, boxheight, WHITE);
      display.setTextColor(BLACK);
      display.setCursor(boxwidth + 2, topHeight);
      display.print("NO");
      display.fillRect(0, topHeight, boxwidth, boxheight, BLACK);
      display.drawRect(0, topHeight, boxwidth, boxheight, WHITE);
      display.setTextColor(WHITE);
      display.setCursor(0, topHeight);
      display.print("YES");
      display.display();
      waitProgramMode = millis() + waitTime;
      putOnQueue(&doNotConfirmProgramModes);
      return;
    }
    if (digitalRead(confirmButton) == LOW)
    {
      Serial.println("rhoh");
      display.clearDisplay();
      display.setTextColor(WHITE, BLACK);
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.print("Mode:");
      display.setCursor(0, topHeight + boxheight);
      display.print("Hrs    Mins");
      display.setTextSize(3);
      display.drawRect(0, topHeight, boxwidth, boxheight, WHITE);
      display.drawRect(boxwidth, topHeight, boxwidth, boxheight, WHITE);
      display.display();
      progenable = 1;
      putOnQueue(&runProgram);
      return;
    }
  }
  putOnQueue(confirmModes);
}

void drawConfirm()
{
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print("Proceed?");
  display.setTextSize(4);
}

void setProgramModes()
{
  display.setTextSize(4);
  if (mode != 4)
    return;
  if (millis() > waitProgramMode)
  {
    if (digitalRead(leftButton) == LOW)
    {
      int oldposition = posposition;
      if (posposition == 0)
      {
        drawConfirm();
        drawYes();
        putOnQueue(confirmModes);
        return;
      }
      else
        --posposition;
      drawNewSelect(posposition, oldposition);
      Serial.println("hey");
      waitProgramMode = millis() + waitTime;
    }
    if (digitalRead(rightButton) == LOW)
    {
      int oldposition = posposition;
      if (posposition == 3)
      {
        drawConfirm();
        drawYes();
        putOnQueue(confirmModes);
        return;
      }
      else
        ++posposition;
      drawNewSelect(posposition, oldposition);
      waitProgramMode = millis() + waitTime;
    }
    if (digitalRead(upButton) == LOW)
    {
      down = modes[posposition];
      modes[posposition] = up;
      if (up == 0)
        up = 3;
      else
        --up;
      drawNewMode();
      waitProgramMode = millis() + waitTime;
    }
    if (digitalRead(downButton) == LOW)
    {
      up = modes[posposition];
      modes[posposition] = down;
      if (down == 3)
        down = 0;
      else
        ++down;
      drawNewMode();
      waitProgramMode = millis() + waitTime;
    }
    if (digitalRead(confirmButton) == LOW)
    {
      times[posposition] = 0;
      if (modes[posposition] == 0)
      {
        drawConfirm();
        drawYes();
        putOnQueue(confirmModes);
        waitProgramMode = millis() + waitTime;
        return;
      }
      putOnQueue(&setProgramTimes);
      display.clearDisplay();
      display.fillRect(0, topHeight, boxwidth, boxheight, WHITE);
      display.setTextColor(BLACK);
      display.setCursor( 0, topHeight);
      display.print("0");
      display.fillRect(boxwidth, topHeight, boxwidth, boxheight, BLACK);
      display.drawRect(boxwidth, topHeight, boxwidth, boxheight, WHITE);
      display.setTextColor(WHITE);
      display.setCursor(boxwidth, topHeight);
      display.print("0");
      display.display();
      waitProgramTime = millis() + waitTime;
      return;
    }
  }
  putOnQueue(&setProgramModes);
}

void setProgram()
{
  if (mode != 4)
    return;
  if (progenable)
  {
    putOnQueue(&runProgram);
    return;
  }
  pos = 0;
  /*
    //Test code.
    modes[0] = 3;
    modes[1] = 2;
    modes[2] = 0;
    times[0] = 1000;
    times[1] = 4000;
    times[2] =  0;
    Serial.println(millis());
    Serial.println(times[0]);
    Serial.println(times[1]);
    Serial.println(times[2]);

    progenable = 1;
    putOnQueue(&runProgram);
  */
  Serial.println("Hello");
  display.clearDisplay();
  posposition = 0;
  modeposition = 0;
  modes[0] = 0;
  display.fillRect(0, topHeight, modeBoxWidth,  modeBoxHeight, WHITE);
  for (int a = 1; a < 4; ++a)
  {
    modes[a] = 0;
    display.drawRect(a * modeBoxWidth, topHeight, modeBoxWidth,  modeBoxHeight, WHITE);
  }
  waitProgramMode = millis() + waitTime;
  display.display();
  putOnQueue(&setProgramModes);
}

void drawCurrentMode(const char* toDraw)
{
  display.clearDisplay();
  display.setTextSize(5);
  display.setCursor(2, 2);
  display.drawRect(0, 0, display.width(), display.height(), WHITE);
  display.print(toDraw);
  display.display();
}

void checkKnob()
{
  static char lasthot = 1;
  static char lastwarm = 1;
  static char lastcool = 1;
  static char lastprogram = 1;
  static unsigned long lastTime = 0;
  static char isSet = 0;
  int hotq = digitalRead(knobHot) == LOW;
  int warmq = digitalRead(knobWarm) == LOW;
  int coolq = digitalRead(knobCool) == LOW;
  int programq = digitalRead(knobProgram) == LOW;
  //please forgive the fact that it appears backwards.  I did not want to mess with my logic.
  int changed = lasthot == hotq && lastwarm == warmq && lastcool == coolq && lastprogram == programq;

  if (changed && !isSet)
  {
    if (lastTime + waitTime < millis())
    {
      display.clearDisplay();
      isSet = 1;
      if (hotq || warmq)
        digitalWrite(bigHeat, HIGH);
      else
        digitalWrite(bigHeat, LOW);
      if (hotq)
      {
        drawCurrentMode("Heat");
        digitalWrite(smaHeat, HIGH);
        mode = 2;
        putOnQueue(&hotReg);
      }
      else
        digitalWrite(smaHeat, LOW);
      if (warmq)
        drawCurrentMode("Warm");
      if (coolq)
      {
        digitalWrite(cooler, HIGH);
        mode = 3;
        drawCurrentMode("Cool");
        putOnQueue(&coolReg);
      }
      else
        digitalWrite(cooler, LOW);

      if (warmq)
        mode = 1;
        //this uses the logic of pressing and holding buttons and this is for a press to activate program mode and stay there.  Unfortunately, this means that sicky logic has been sacrificed for now.
      if (programq)
      {
        mode = 4;
        putOnQueue(&setProgram);
        Serial.println("setting program");
        return;
      }
      else
      {
        progenable = 0;
      }

    }
    display.display();
  } else
  { if (!changed)
    {
      isSet = 0;
      lastTime = millis();
      lasthot = hotq;
      lastwarm = warmq;
      lastcool = coolq;
      lastprogram = programq;
    }
  }
  putOnQueue(knobCheck);
}


void setup()
{
  // put your setup code here, to run once:
  pinMode(knobHot, INPUT_PULLUP);
  pinMode(knobProgram, INPUT_PULLUP);
  pinMode(knobWarm, INPUT_PULLUP);
  pinMode(knobCool, INPUT_PULLUP);
  pinMode(upButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);
  pinMode(leftButton, INPUT_PULLUP);
  pinMode(rightButton, INPUT_PULLUP);
  pinMode(confirmButton, INPUT_PULLUP);

  pinMode(bigHeat, OUTPUT);
  pinMode(smaHeat, OUTPUT);
  pinMode(cooler, OUTPUT);

  Serial.begin(57600);
  Serial.println(SSD1306_LCDHEIGHT);
  //screen set up code.
  display.begin(SSD1306_SWITCHCAPVCC);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  knobCheck = &checkKnob;
  confirmModes = &confirmProgramModes;
  putOnQueue(knobCheck);
}

//I present to you: a very simple cooperative multitasking system.
void loop()
{
  if (queue[queueRead] != NULL)
  {
    queue[queueRead]();
    queue[queueRead] = NULL;
    if (queueRead == 31)
      queueRead = 0;
    else
      ++queueRead;
  }
  else
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println("Runtime error.  Restart the Cook and Cool");
    display.display();
    while (1);
  }
}
