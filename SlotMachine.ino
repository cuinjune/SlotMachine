
#include "LedControl.h"
#include "Servo.h" 

LedControl lc = LedControl(12,10,11,4);  // Pins: DIN,CLK,CS, # of Display connected
int modula(int a, int b)
{
    int result;
    if (b < 0) b = -b;
    else if (!b) b = 1;
    result = a % b;
    if (result < 0) result += b;
    return result;
}
float getEaseInOutCirc(float currentTime, float duration, float beginValue, float endValue)
{
    float changeInValue = endValue - beginValue;
    currentTime /= duration / 2;
    if (currentTime < 1) return -changeInValue/2 * (sqrtf(1-currentTime * currentTime) - 1) + beginValue;
    currentTime -= 2;
    float result = changeInValue / 2 * (sqrtf(1 - currentTime * currentTime) + 1) + beginValue;
    if (endValue - result <= 1)
    result = endValue - 1;
    return result;
}

byte images[40] =
{
    // seven
    B00000000,
    B11111111,
    B00001110,
    B00011100,
    B00011000,
    B00011000,
    B00011000,
    B00011000,

    // alien4
    B00000000,
    B01000010,
    B00111100,
    B01011010,
    B11111111,
    B10000001,
    B11111111,
    B10100101,

    // dog
    B00000000,
    B10100010,
    B11100001,
    B11100010,
    B11111110,
    B11111110,
    B10101010,
    B10101010,

    // heart
    B00000000,
    B01100110,
    B11111111,
    B11111111,
    B11111111,
    B01111110,
    B00111100,
    B00011000,

    // skull
    B00000000,
    B00111100,
    B01111110,
    B10111101,
    B10011001,
    B01111110,
    B01000010,
    B01111110
};

const int leverPin = 2;
int previousLeverState = 0;
const int frameRate = 60;
unsigned long lastFrameTime;
unsigned long triggerTime;
int gameResult = 0;
unsigned long gameResultPreviousTime;
bool gameResultBlinkMode = false;

enum Stage{STAGE_STOPPED, STAGE_TRIGGERED, STAGE_ROLLING};

class LedMatrix
{
public:
    unsigned long offsetY, aliasY, targetY;
    int kind;
    int duration;
    Stage stage;
private:
};

LedMatrix *ledMatrix[3];

// Declare the Servo pin
int servoPin = 3;

// Create a servo object
Servo servo;  

class SquareOsc
{
public:
    SquareOsc(const float frequency)
    :frequency(frequency){};
    virtual ~SquareOsc(){};
    void setFrequency(const float frequency)
    {
        this->frequency = frequency;
    }
    int process()
    {
        const unsigned long currentTimeMicros = micros();
        const unsigned long elapsedTimeMicros = currentTimeMicros - lastTimeMicros;
        const unsigned long cycleDurationMicros = static_cast<unsigned long>(1000.0f / frequency) * 1000;
        if (elapsedTimeMicros < cycleDurationMicros)
        {
            int value;
            if (elapsedTimeMicros < cycleDurationMicros / 2)
                value = 1;
            else
                value = -1;
            if (value == previousValue)
            {
                return 0;
            }
            previousValue = value;
            return value;
        }
        else
        {
            lastTimeMicros = currentTimeMicros;
            return 0;
        }
    }
private:
    float frequency;
    unsigned long lastTimeMicros;
    int previousValue;
};

// Declare the Audio out pin
int audioOutPin = 9;

SquareOsc *squareMain, *squareLFO;

float mtof(const float note)
{
    if (note <= -1500) return 0;
    else if (note > 1499) return mtof(1499);
    else return 8.17579891564 * exp(.0577622650 * note);
}

//--------------------------------------------------------------
void setup()
{
    //basic settings
    randomSeed(micros()); //seed random
    for (int i = 2; i >= 0; --i)
    {
        lc.shutdown(i, false);
        lc.setIntensity(i, 15); //set the brightness
        lc.clearDisplay(i); //switch all leds off
        ledMatrix[i] = new LedMatrix();
        ledMatrix[i]->offsetY = random(0, 5) * 8;
        ledMatrix[i]->duration = 4000 + (2 - i) * 1000;
        ledMatrix[i]->stage = STAGE_STOPPED;
        for (int j = 0; j < 8; ++j)
        {
            int index = modula(j - ledMatrix[i]->offsetY % 40, 40);
            lc.setRow(i, j, images[index]);
        }
    }
    lastFrameTime = millis();
    triggerTime = -10000;
    gameResult = 0;
    
    servo.attach(servoPin);
    servo.write(0);
    
    pinMode(audioOutPin, OUTPUT);
    squareMain = new SquareOsc(220.f);
    squareLFO = new SquareOsc(15.f);
}

//--------------------------------------------------------------
void loop()
{
    if (ledMatrix[0]->stage == STAGE_TRIGGERED)
    {
        squareMain->setFrequency(40);
        int mainValue = squareMain->process();
        if (mainValue)
        {
           digitalWrite(audioOutPin, (mainValue + 1) / 2);
        }
    }
    else if (ledMatrix[0]->stage == STAGE_ROLLING)
    {
      int lfoValue = squareLFO->process();
      if (lfoValue)
      {
          squareMain->setFrequency(lfoValue * 50 + 330);
      }
      int mainValue = squareMain->process();
      if (mainValue)
      {
          digitalWrite(audioOutPin, (mainValue + 1) / 2);
      }
    }
    int newLeverState = digitalRead(leverPin);
    if (ledMatrix[0]->stage == STAGE_STOPPED &&
        ledMatrix[1]->stage == STAGE_STOPPED &&
        ledMatrix[2]->stage == STAGE_STOPPED &&
        previousLeverState == 0 && newLeverState == 1 &&
        gameResult == 0)
    {
        int numRetries = 0;
        for (int i = 2; i >= 0; --i)
        {
            ledMatrix[i]->stage = STAGE_TRIGGERED;
            if (i == 2)
            {
                ledMatrix[i]->targetY = ledMatrix[i]->offsetY + 320 + (2 - i) * 320 + random(0, 5) * 8;
                ledMatrix[i]->kind = (ledMatrix[i]->targetY % 40) / 8;
                if (ledMatrix[i]->kind == 0) //seven
                    numRetries = 1;
                else if (ledMatrix[i]->kind == 2) //heart
                    numRetries = 3;
                else
                    numRetries = 5;
            }
            else
            {
                ledMatrix[i]->targetY = ledMatrix[i]->offsetY + 320 + (2 - i) * 320 + random(0, 5) * 8;
                ledMatrix[i]->kind = (ledMatrix[i]->targetY % 40) / 8;
                while (ledMatrix[i]->kind != ledMatrix[2]->kind && numRetries)
                {
                    ledMatrix[i]->targetY = ledMatrix[i]->offsetY + 320 + (2 - i) * 320 + random(0, 5) * 8;
                    ledMatrix[i]->kind = (ledMatrix[i]->targetY % 40) / 8;
                    numRetries--;
                }
            }
        }
        triggerTime = millis();
    }
    previousLeverState = newLeverState;
    unsigned long elapsed = millis() - lastFrameTime;
    if (elapsed > 1000.f / frameRate)
    {
        unsigned long currentTime = millis() - triggerTime;
        for (int i = 2; i >= 0; --i)
        {
            if (ledMatrix[i]->stage != STAGE_STOPPED)
            {
                if (currentTime < ledMatrix[i]->duration)
                {
                    ledMatrix[i]->stage = STAGE_ROLLING;
                    ledMatrix[i]->aliasY = static_cast<unsigned long>(getEaseInOutCirc(currentTime, ledMatrix[i]->duration, ledMatrix[i]->offsetY, ledMatrix[i]->targetY) + 1);
                }
                else if (ledMatrix[i]->stage == STAGE_ROLLING)
                {
                    ledMatrix[i]->stage = STAGE_STOPPED;
                    ledMatrix[i]->offsetY = ledMatrix[i]->aliasY;
                    if (i == 0 && ledMatrix[0]->kind == ledMatrix[1]->kind &&
                        ledMatrix[0]->kind == ledMatrix[2]->kind &&
                        ledMatrix[1]->kind == ledMatrix[2]->kind)
                    {
                        if (ledMatrix[0]->kind == 0) //heart
                            gameResult = 5;
                        else if (ledMatrix[0]->kind == 2) //seven
                            gameResult = 3;
                        else
                            gameResult = 1;
                        randomSeed(micros());
                        gameResultPreviousTime = millis();
                        break;
                    }
                }
                for (int j = 0; j < 8; ++j)
                {
                    int index = modula(j - ledMatrix[i]->aliasY % 40, 40);
                    lc.setRow(i, j, images[index]);
                }
            }
        }
        
        if (gameResult)
        {
            unsigned long gameResultElapsedTime = millis() - gameResultPreviousTime;
            if (!gameResultBlinkMode && gameResultElapsedTime > 1300)
            {
                for (int i = 2; i >= 0; --i)
                {
                    lc.clearDisplay(i); //switch all leds off
                }
                gameResultBlinkMode = true;
                servo.write(180);
                tone(audioOutPin, mtof(53 + 12), 150);
               delay(200);
               tone(audioOutPin, mtof(60 + 12), 150);
               delay(200);
               tone(audioOutPin, mtof(69 + 12), 150);
               delay(200);
               tone(audioOutPin, mtof(65 + 12), 150);
               delay(200);
               tone(audioOutPin, mtof(72 + 12), 150);
               delay(200);
               noTone(audioOutPin);   
            }
            else if (gameResultBlinkMode && gameResultElapsedTime > 2300)
            {
                for (int i = 2; i >= 0; --i)
                {
                    for (int j = 0; j < 8; ++j)
                    {
                        int index = modula(j - ledMatrix[i]->aliasY % 40, 40);
                        lc.setRow(i, j, images[index]);
                    }
                }
                gameResultBlinkMode = false;
                gameResultPreviousTime = millis();
                gameResult--;
                servo.write(0);

                 
            }
        }
        lastFrameTime = millis();
    }
}
