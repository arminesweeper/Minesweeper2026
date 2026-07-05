#include <PID_v1.h>

// ===== Configuration =====
constexpr uint8_t PWM_R=9, DIR_R=12, PWM_L=11, DIR_L=7;
constexpr uint8_t ENC_RA=3, ENC_RB=5, ENC_LA=2, ENC_LB=4;
constexpr uint16_t ENCODER_PPR=385;
constexpr uint16_t CONTROL_MS=100;
constexpr uint16_t CMD_TIMEOUT_MS=500;
constexpr uint8_t PWM_MIN=30;

volatile uint16_t rightCount=0,leftCount=0;
volatile int8_t rightSign=1,leftSign=1;

double spR=0,spL=0,inR=0,inL=0,outR=0,outL=0;
PID pidR(&inR,&outR,&spR,11.5,7.5,0.1,DIRECT);
PID pidL(&inL,&outL,&spL,12.8,8.3,0.1,DIRECT);

unsigned long lastCtrl=0,lastCmd=0;
char buf[32]; uint8_t idx=0;

void isrR(){ rightSign=digitalRead(ENC_RB)?1:-1; rightCount++; }
void isrL(){ leftSign=digitalRead(ENC_LB)?-1:1; leftCount++; }

void stopMotors(){analogWrite(PWM_R,0);analogWrite(PWM_L,0);spR=spL=0;}

void setup(){
 pinMode(PWM_R,OUTPUT); pinMode(DIR_R,OUTPUT);
 pinMode(PWM_L,OUTPUT); pinMode(DIR_L,OUTPUT);
 pinMode(ENC_RB,INPUT); pinMode(ENC_LB,INPUT);
 attachInterrupt(digitalPinToInterrupt(ENC_RA),isrR,RISING);
 attachInterrupt(digitalPinToInterrupt(ENC_LA),isrL,RISING);
 digitalWrite(DIR_R,HIGH); digitalWrite(DIR_L,HIGH);
 Serial.begin(115200);
 pidR.SetMode(AUTOMATIC); pidL.SetMode(AUTOMATIC);
 pidR.SetOutputLimits(0,255); pidL.SetOutputLimits(0,255);
 pidR.SetSampleTime(CONTROL_MS); pidL.SetSampleTime(CONTROL_MS);
}

void parseLine(char*s){
 char m,dir; float vel;
 if(sscanf(s,"%c,%c,%f",&m,&dir,&vel)==3){
   bool f=dir=='+';
   if(m=='R'){ digitalWrite(DIR_R,f); spR=vel; }
   if(m=='L'){ digitalWrite(DIR_L,f); spL=vel; }
   lastCmd=millis();
 }
}

void loop(){
 while(Serial.available()){
   char c=Serial.read();
   if(c=='\n'){ buf[idx]=0; parseLine(buf); idx=0; }
   else if(idx<31) buf[idx++]=c;
 }
 if(millis()-lastCmd>CMD_TIMEOUT_MS) stopMotors();
 if(millis()-lastCtrl>=CONTROL_MS){
   lastCtrl=millis();
   noInterrupts();
   uint16_t rc=rightCount, lc=leftCount;
   int8_t rs=rightSign, ls=leftSign;
   rightCount=leftCount=0;
   interrupts();
   float rpmR=rc*60000.0/(CONTROL_MS*ENCODER_PPR);
   float rpmL=lc*60000.0/(CONTROL_MS*ENCODER_PPR);
   inR=rs*rpmR*0.104719755;
   inL=ls*rpmL*0.104719755;
   pidR.Compute(); pidL.Compute();
   if(spR==0) outR=0; if(spL==0) outL=0;
   uint8_t pwmR=outR?max((int)PWM_MIN,(int)outR):0;
   uint8_t pwmL=outL?max((int)PWM_MIN,(int)outL):0;
   analogWrite(PWM_R,pwmR); analogWrite(PWM_L,pwmL);
   Serial.print("R,");Serial.print(inR,3);Serial.print(",L,");Serial.println(inL,3);
 }
}
