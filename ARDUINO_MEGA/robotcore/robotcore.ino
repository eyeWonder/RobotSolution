// COMANDI PER TEST: 
//AVANTI 20 E INDIETRO 20: 
//19,20;19,-20;


//////////////////////////////////////////////////////////////////////////////////
// CONFIGURAZIONE DEL SISTEMA                  ///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#pragma region CONFIGURAZIONE DEL SISTEMA   
//#define delay(ms) chThdSleepMilliseconds(ms) 

#include <MyRobotLibs\dbg.h>

#include <MyRobotLibs\systemConfig.h>
#include <MyRobotLibs\hw_config.h>

#pragma endregion
// ////////////////////////////////////////////////////////////////////////////////////////////
// ///																						///
// ///       LIBRERIE 																		///
// ///		Aggiungere ciascun percorso nelle proprietà del progetto in Visual Studio 		///
// ///		Configuration Properties >C++ > Path											///
// ////////////////////////////////////////////////////////////////////////////////////////////
#pragma region LIBRERIE
/// ///////////////////////////////////////////////////////////////////////////////
// ROS
/// ///////////////////////////////////////////////////////////////////////////////
#include <ros.h>
#include <ros/duration.h>
#include <ros/time.h> //non serve
#include <ros/msg.h>
#include <ros_lib\sensor_msgs\Range.h>
#include <std_msgs/String.h>
#include <std_msgs/Empty.h>

#include <sensor_msgs/Range.h>
#include <tf/transform_broadcaster.h>


#include <digitalWriteFast.h>
#include <ChibiOS_AVR.h>
#include <util/atomic.h>
//#include <TinyGPSplus/TinyGPS++.h>
//#include <StackArray.h>
#include <avr/interrupt.h>


#include <Arduino.h>	//per AttachInterrupt



#pragma endregion

// ////////////////////////////////////////////////////////////////////////////////////////////
//  CREAZIONE OGGETTI GLOBALI
// ////////////////////////////////////////////////////////////////////////////////////////////
#pragma region CREAZIONE OGGETTI GLOBALI
#if OPT_MPU6050
	#include <I2Cdev\I2Cdev.h>

	#include <MPU6050\MPU6050_6Axis_MotionApps20.h>
//	#include <MPU6050\MPU6050.h> // not necessary if using MotionApps include file

	// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
	// is used in I2Cdev.h
	#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
	#include "Wire.h"
	#endif

	// class default I2C address is 0x68
	// specific I2C addresses may be passed as a parameter here
	// AD0 low = 0x68 (default for SparkFun breakout and InvenSense evaluation board)
	// AD0 high = 0x69
	MPU6050 mpu;
	//MPU6050 mpu(0x69); // <-- use for AD0 high


	// MPU control/status vars
	bool dmpReady = false;  // set true if DMP init was successful
	uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
	uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
	uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
	uint16_t fifoCount;     // count of all bytes currently in FIFO
	uint8_t fifoBuffer[64]; // FIFO storage buffer

							// orientation/motion vars
	Quaternion q;           // [w, x, y, z]         quaternion container
	VectorInt16 aa;         // [x, y, z]            accel sensor measurements
	VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
	VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
	VectorFloat gravity;    // [x, y, z]            gravity vector
	float euler[3];         // [psi, theta, phi]    Euler angle container
	float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

#endif // COMPASS
#if OPT_COMPASS
	#include <Wire\Wire.h>
	#include <compass\compass.h>
	MyCompass_c compass;
	//#include <Adafruit_Sensor\Adafruit_Sensor.h> //richiesto dalla liberia compass Adafruit_HMC5883_U
	//#include <HMC5883L\HMC5883L.h>
	//HMC5883L compass;
#endif // COMPASS
#if OPT_SONAR
	#include <Newping\NewPing.h>
	NewPing sonar(Pin_SonarTrig, Pin_SonarEcho);


#endif //
#if OPT_SERVOSONAR
	// va messo prima dell'istanza del robot
	#include <Servo\src\Servo.h>
	#include <Servo.h> //deve restare qui altrimenti il linker s'incazza (??)

	#include <PWM\PWM.h>
	Servo servoSonar;
#endif

#if OPT_GPS
	#include <TinyGPSplus\TinyGPS++.h> //deve essere incluso anche nel main

	TinyGPSPlus Gps;
#endif

#pragma region VL53L0X distanceSensor

#if OPT_LDS
	#include <Wire\Wire.h>
	#include <VL53L0X\VL53L0X.h>
	VL53L0X LDS;
	// Uncomment this line to use long range mode. This
	// increases the sensitivity of the distanceSensor and extends its
	// potential range, but increases the likelihood of getting
	// an inaccurate reading because of reflections from objects
	// other than the intended target. It works best in dark
	// conditions.
	#define LONG_RANGE
	// Uncomment ONE of these two lines to get
	// - higher speed at the cost of lower accuracy OR
	// - higher accuracy at the cost of lower speed

	//#define HIGH_SPEED
	//#define HIGH_ACCURACY

#endif // OPT_LDS



#if OPT_STEPPERLDS
	#include <NewTone\NewTone.h> // PER GENERARE IL CLOCK DELLO STEPPER LDS
	stepper_c myLDSstepper;
#endif


#pragma endregion

#include <robot.h>
struct robot_c robot;	//was  struct robot_c robot;


// ////////////////////////////////////////////////////////////////////////////////////////////


// ////////////////////////////////////////////////////////////////////////////////////////////
//  CmdMessenger object to the default Serial port
// ////////////////////////////////////////////////////////////////////////////////////////////
#include <CmdMessenger2/CmdMessenger2.h>
static CmdMessenger2 cmdMMI = CmdMessenger2(SERIAL_MMI);
static CmdMessenger2 cmdPC = CmdMessenger2(SERIAL_MSG);
#include <MyRobotLibs\RobotInterfaceCommands2.h>
// usare le macro  MSG per inviare messaggi sia su Serial_PC, sia Serial_MMI
//------------------------------------------------------------------------------
#pragma region DEFINIZIONE MAILBOX VOICE
// mailbox size and memory pool object count
const size_t MBOX_COUNT = 6;

// type for a memory pool object
struct mboxObject_t {
	char* name;
	char str[100];
	int size;
};
// array of memory pool objects
mboxObject_t msgObjArray[MBOX_COUNT];

// memory pool structure
//MEMORYPOOL_DECL(memPool, MBOX_COUNT, 0);

// slots for mailbox messages
//msg_t letter[MBOX_COUNT];

// mailbox structure
//MAILBOX_DECL(mailVoice, &letter, MBOX_COUNT);





/// ///////////////////////////////////////////////////////////////////////////////
// M U T E X ///////////////////////////////////
/// ///////////////////////////////////////////////////////////////////////////////
// Mutex for atomic access to data.
MUTEX_DECL(mutexMotion); //condiviso dai comandi di movimento e sonar per non essere in movimento quando il sonar scansiona
MUTEX_DECL(mutexSerialMMI);// accesso alla seriale
MUTEX_DECL(mutexSerialPC);// accesso alla seriale
MUTEX_DECL(mutexSensors);// accesso ai sensori in lettura e scrittura

#pragma endregion

// ////////////////////////////////////////////////////////////////////////////////////////////

#pragma endregion

// ////////////////////////////////////////////////////////////////////////////////////////////
//  FUNZIONI E UTILITIES GLOBALI
// ////////////////////////////////////////////////////////////////////////////////////////////
//void playSingleNote(int pin, int freq,int noteDuration) {
//	tone(pin, freq, noteDuration);
//	noTone(pin);
//}
//Dichiarazione di funzione che punta all’indirizzo zero
void(*softReset)(void) = 0;

//volatile bool interruptFlag = 0;



// ////////////////////////////////////////////////////////////////////////////////////////////
///  bloccante
void lampeggiaLed(int pin, int freq, uint16_t nvolte) {
	int ms = 500 / freq;
	for (size_t i = 0; i < nvolte; i++)
	{
		digitalWriteFast(pin, 1);
		chThdSleepMilliseconds(ms);
		digitalWriteFast(pin, 0);
		chThdSleepMilliseconds(ms);

	}

}
// ////////////////////////////////////////////////////////////////////////////////////////////

void countDown(int seconds) {
	MSG3("CountDown in ", seconds," sec...");
	for (size_t i = seconds; i > 0; i--)
	{
		MSG2("  -",i);
		delay(1000);
	}
}
// ////////////////////////////////////////////////////////////////////////////////////////////
#pragma region ROS


#pragma endregion
// ////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//                   /////////////////////////////////////////////////////////////
// PROCESSI CHIBIOS  /////////////////////////////////////////////////////////////
//                   /////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// thread 1		- Esplora
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
/*
static THD_WORKING_AREA(waThreadEsplora, 400);
static THD_FUNCTION(ThreadEsplora, arg) {
const int FWDIST = 10;
int alfa = 0;
int cmDone = 0; // percorso eseguito a valle di un comando moveCm o RotateDeg
int stuckCount = 0;

while (robot.status.operatingMode == AUTONOMOUS)
{

TOGGLEPIN(Pin_LED_TOP_B);
robot.status.parameters.sonarStartAngle = 0;
robot.status.parameters.sonarEndAngle = 180;
robot.status.parameters.sonarStepAngle = 30;
robot.status.parameters.sonarScanSweeps = 1;
robot.status.parameters.sonarMedianSamples = 2;
robot.status.parameters.sonarScanSpeed = 30; // map(analogRead(Pin_AnaPot1), 0, 1023, 10, 500);  //was = 30 ms di attesa tra due posizioni

robot.SonarScanBatch(&servoSonar, &Sonar);
alfa = 90 - robot.status.parameters.SonarMaxDistAngle;
SERIAL_MSG.print("Max dist @alfa:"); SERIAL_MSG.println(alfa);
dbg2("Max dist cm:", robot.status.parameters.sonarMaxDistance)
TOGGLEPIN(Pin_LED_TOP_B);

// invia i dati Sonar
kbSonarSendData(&cmdMMI);
TOGGLEPIN(Pin_LED_TOP_B);


robot.rotateDeg(alfa);
cmDone = robot.moveCm(robot.status.parameters.sonarMaxDistance);	// avanti
if (cmDone < (robot.status.parameters.sonarMaxDistance - 1))	//ostacolo ?
{
SERIAL_MSG.println("1,Obst!;");
robot.moveCm(-FWDIST);	// torna indietro
TOGGLEPIN(Pin_LED_TOP_B);
stuckCount++;
if (stuckCount>2)
{
robot.rotateDeg(180); //inverto la direzione
TOGGLEPIN(Pin_LED_TOP_B);
stuckCount = 0;
}
}
else //nessun ostacolo, azzero il contatore
{
stuckCount = 0;
}
TOGGLEPIN(Pin_LED_TOP_B);


chThdSleepMilliseconds(1500);	// Sleep for 150 milliseconds.

}
//  return 0;
}
*/


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// thread   - SAFETY
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
static THD_WORKING_AREA(waSafety, 100);
static THD_FUNCTION(thdSafety, arg) {
	//robot.status.sensors.ignoreIR = true;

	while (true)
	{
		if (robot.status.isMoving && !robot.isObstacleFree())
		{
			robot.stop();
			MSG("Obstacle-Stop motor")
		}
		chThdSleepMilliseconds(200);
	}
}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// thread   - lettura sensori HR in robot.status
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

static THD_WORKING_AREA(waReadSensorsHR, 400);
static THD_FUNCTION(thdReadSensorsHR, arg) {
//	chMtxLock(&mutexSensors);
	// Allo startup leggo tutti isensori
	robot.readAllSensors();	//IR proxy, Gyro, GPS
//	chMtxUnlock(&mutexSensors);

	while (1)
	{
		LEDTOP_B_ON

//		chMtxLock(&mutexSensors);
		MSG("S HR>")
		robot.readSensorsHR();	//IR proxy, Gyro, GPS
		robot.status.tictac = !robot.status.tictac;
//		chMtxUnlock(&mutexSensors);
		LEDTOP_B_OFF


		//yeld in base alla modalità operativa
		if (robot.status.operatingMode == MODE_AUTONOMOUS) {
			chThdSleepMilliseconds(500);// Sleep for n milliseconds.
		}
		else {
			chThdSleepMilliseconds(500);
		} 


	}
	//  return 0;
}
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// thread 2B - lettura sensori LR in robot.status
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
static THD_WORKING_AREA(waReadSensorsLR, 400);
static THD_FUNCTION(thdReadSensorsLR, arg) {
	chMtxLock(&mutexSensors);
	// Allo startup leggo tutti isensori
	robot.readAllSensors();	//IR proxy, Gyro, GPS
//	chMtxUnlock(&mutexSensors);

	while (1)
	{
		LEDTOP_B_ON

//		chMtxLock(&mutexSensors);
		MSG("S LR>")
		robot.readSensorsLR();	//IR proxy, Gyro, GPS
		robot.status.tictac = !robot.status.tictac;
//		chMtxUnlock(&mutexSensors);
 
		LEDTOP_B_OFF

		//yeld in base alla modalità operativa
		if (robot.status.operatingMode == MODE_AUTONOMOUS) {
			chThdSleepMilliseconds(3000);// Sleep for n milliseconds.
		}
		else {
			chThdSleepMilliseconds(5000);
		} 


	}
	//  return 0;
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// thread   MMIcommands - ricezione comandi da MMI
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
static THD_WORKING_AREA(waMMIcommands, 200);
static THD_FUNCTION(thdMMIcommands, arg) {
	//static msg_t MMIcommands(void *arg)


	// Setup CommandMessenger -----------------------------------------------------
	cmdMMI.printLfCr();   // Adds newline to every command 
	attachCommandCallbacks(&cmdMMI);// Attach my application's user-defined callback methods

	while (true)
	{
		dbg("1,MMI>; ");

		LEDTOP_B_ON

		///osalSysDisable(); //disabilita Interupt
		cmdMMI.feedinSerialData(); // Comando da interfaccia MMI ?, Se sì lo esegue
		///osalSysEnable();//abilita Interupt
		LEDTOP_B_OFF

			chThdSleepMilliseconds(900);
		//if (robot.operatingMode == SLAVE) 			{chThdSleepMilliseconds(200);}// Sleep for n milliseconds.
		//else if (robot.operatingMode == AUTONOMOUS)	{chThdSleepMilliseconds(500);}// Sleep for n milliseconds.
		//else										{chThdSleepMilliseconds(100);}// Sleep for n milliseconds.
	}
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// thread   PCcommands - ricezione comandi da PC (per Debug)
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
static THD_WORKING_AREA(waPCcommands, 100);
static THD_FUNCTION(thdPCcommands, arg) {
	// Setup CommandMessenger -----------------------------------------------------
	cmdPC.printLfCr();   // Adds newline to every command 
	attachCommandCallbacks(&cmdPC);// Attach my application's user-defined callback methods
	MSG("1,thdPCcommands STARTED;");

	while (true)
	{
		dbg("1,PC>; ");


		//osalSysDisable(); //disabilita Interupt
//		chMtxLock(&mutexSerialPC);
		cmdPC.feedinSerialData();  // Comando da pc ?, Se sì lo esegue
//		chMtxUnlock(&mutexSerialPC);
		//osalSysEnable();//abilita Interupt


		//yeld in base alla modalità operativa
		if (robot.status.operatingMode == MODE_SLAVE) {
			chThdSleepMilliseconds(500);// Sleep for n milliseconds.
		}
		else {
			chThdSleepMilliseconds(1000);
		}// Sleep for n milliseconds.

	}
}
// invia un  messaggio con la descrizione dell'evento e lo resetta 
void msgEventHR() {
	if (robot.status.pendingEvents.bumperF) { MSG("bumperF  EVENT");robot.status.pendingEvents.bumperF = false; }
	if (robot.status.pendingEvents.bumperL) { MSG("bumperL  EVENT");robot.status.pendingEvents.bumperL = false; }
	if (robot.status.pendingEvents.bumperR) { MSG("bumperR  EVENT");robot.status.pendingEvents.bumperR = false; }
	if (robot.status.pendingEvents.EncL) { MSG("EncL  EVENT");robot.status.pendingEvents.EncL = false; }
	if (robot.status.pendingEvents.EncR) { MSG("EncR  EVENT");robot.status.pendingEvents.EncR = false; }
	if (robot.status.pendingEvents.irproxyB) { MSG("irproxyB  EVENT");robot.status.pendingEvents.irproxyB = false; }
	if (robot.status.pendingEvents.irproxyF) { MSG("irproxyF  EVENT");robot.status.pendingEvents.irproxyF = false; }
	if (robot.status.pendingEvents.irproxyFH) { MSG("irproxyFH  EVENT");robot.status.pendingEvents.irproxyFH = false; }
	if (robot.status.pendingEvents.irproxyL) { MSG("irproxyL  EVENT");robot.status.pendingEvents.irproxyL = false; }
	if (robot.status.pendingEvents.irproxyR) { MSG("irproxyR  EVENT");robot.status.pendingEvents.irproxyR = false; }

	if (robot.status.pendingEvents.pirDome) { MSG("pirDome  EVENT");robot.status.pendingEvents.pirDome = false; }
	//if (robot.status.pendingEvents.analog[0]) { MSG("POT EVENT"); robot.status.pendingEvents.analog[0]= false;	}
	//if (robot.status.pendingEvents.analog[2]) { MSG("LIGHT EVENT");robot.status.pendingEvents.analog[2]= false;	 }
	
	
	
	
	
	
	
	
	
	
	
	
	

}
// se pending invia messaggio di evento su Batteria,luca e GPS
void msgEventLR() {

	if (robot.status.pendingEvents.batCharge) { MSG("BATTERY EVENT");robot.status.pendingEvents.batCharge= false;	 }
	if (robot.status.pendingEvents.light) { MSG("light  EVENT"); robot.status.pendingEvents.light = false;}
	if (robot.status.pendingEvents.gps) { MSG("GPS  EVENT"); robot.status.pendingEvents.gps = false;}

}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// thread   SendStatusHR - invia lo stato sui vari canali
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
static THD_WORKING_AREA(waSendStatusHR, 100);
static THD_FUNCTION(thdSendStatusHR, arg) {
	while (true)
	{
		if (robot.status.pendingEvents.EventFlag)
		{
			LEDTOP_B_ON;
			// wait to enter print region
			//chMtxLock(&mutexSerialMMI);
			msgEventHR();

			osalSysDisable(); //disabilita Interupt
			onCmdGetSensorsHRate(&cmdMMI);
			onCmdGetSensorsLRate(&cmdMMI);
			osalSysEnable();//abilita Interupt

			//chMtxUnlock(&mutexSerialMMI);


			//chMtxLock(&mutexSerialPC);
			onCmdGetSensorsHRate(&cmdPC);
			onCmdGetSensorsLRate(&cmdPC);
			//chMtxUnlock(&mutexSerialPC);



			LEDTOP_B_OFF

		}

		chThdSleepMilliseconds(800);// Sleep for n milliseconds.


	}
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// thread   SendStatusLR - invia lo stato sui vari canali
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
static THD_WORKING_AREA(waSendStatusLR, 200);
static THD_FUNCTION(thdSendStatusLR, arg) {

	//chMtxLock(&mutexSerialMMI); //si blocca finchè la seriale è in uso da un altro thread
	//onCmdGetSensorsLRate(&cmdMMI);
	//chMtxUnlock(&mutexSerialMMI);

	//chMtxLock(&mutexSerialPC); //si blocca finchè la seriale è in uso da un altro thread
	//onCmdGetSensorsLRate(&cmdPC);
	//chMtxUnlock(&mutexSerialPC);

	while (true)
	{
		LEDTOP_B_ON
			if ((robot.status.operatingMode == MODE_AUTONOMOUS) &&
		 ( robot.status.pendingEvents.light
			|| robot.status.pendingEvents.batCharge
			|| robot.status.pendingEvents.gps
			))
		{
				//osalSysDisable(); //disabilita Interupt
				// wait to enter print region
				//chMtxLock(&mutexSerialMMI);
				msgEventLR();


			robot.status.pendingEvents.gps = false;
			robot.status.pendingEvents.batCharge = false;
			robot.status.pendingEvents.light = false;

			//		chMtxLock(&mutexSerialMMI); //si blocca finchè la seriale è in uso da un altro thread
			onCmdGetSensorsLRate(&cmdMMI);
			//		chMtxUnlock(&mutexSerialMMI);

			//		chMtxLock(&mutexSerialPC); //si blocca finchè la seriale è in uso da un altro thread
			onCmdGetSensorsLRate(&cmdPC);
			//		chMtxUnlock(&mutexSerialPC);

		}



		LEDTOP_B_OFF

		//yeld in base alla modalità operativa
		if (robot.status.operatingMode == MODE_AUTONOMOUS) {
			#ifdef DEBUG_ON
				chThdSleepMilliseconds(2000);// Sleep for n milliseconds.
			#else
				chThdSleepMilliseconds(5000);// Sleep for n milliseconds.
			#endif
		}
		else { //SLEEP TIME IN MODALITA' SLAVE O JOISTICK
			#ifdef DEBUG_ON
				chThdSleepMilliseconds(2000);// Sleep for n milliseconds.
			#else
				chThdSleepMilliseconds(15000);// Sleep for n milliseconds.
			#endif
		}


	}
}

//////////////////////////////////////////////////////////////////////////////////
//  THREAD  B R A I N      ///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#pragma region  Processo	B R A I N    
static THD_WORKING_AREA(waBrain, 100);
static THD_FUNCTION(thdBrain, arg)
{
	// Setup CommandMessenger -----------------------------------------------------
	cmdMMI.printLfCr();   // Adds newline to every command 
	attachCommandCallbacks(&cmdMMI);// Attach my application's user-defined callback methods

	int sleepTime = 500;
	const int FWDIST = 10; //distanza avanzamento
	int alfa = 0;
	int cmDone = 0; // percorso eseguito a valle di un comando moveCm o RotateDeg
	int stuckCount = 0;

	//1)inizializzo i sensori e le variabili
	int pingCm = 0;
	int ldsmm = 0;
	int estimatedDistCm = 0;
 
	MSG2("OPMODE..",(int)robot.status.operatingMode);

	robot.status.cmd.commandDir = commandDir_e::GOR;

	while (1)
	{
		LEDTOP_G_ON
		switch (robot.status.operatingMode)
		{
		case operatingMode_e::MODE_SLAVE:
			#pragma region SLAVE

				MSG("B s>")

///				osalSysDisable(); //disabilita Interupt
				cmdMMI.feedinSerialData(); // Comando da interfaccia MMI ?, Se sì lo esegue
//				osalSysEnable();//abilita Interupt


				//Invia i dati dei sensori se cambia qualcosa
				if (robot.raiseEvents()) {

					msgEventHR();
					msgEventLR();
					robot.resetEvents();
				}
				//MSG(" okei comanda");
				sleepTime = 1000;
				break;
			#pragma endregion

		case	operatingMode_e::MODE_AUTONOMOUS:

		#pragma region AUTONOMOUS
			sleepTime = 2000;
			MSG("B A>")


			onCmdGetSensorsHRate(&cmdMMI);
			onCmdGetSensorsHRate(&cmdPC);
			onCmdGetSensorsLRate(&cmdMMI);
			onCmdGetSensorsLRate(&cmdPC);
			onCmdGetProxy(&cmdMMI);

			// ////////////////////////////////////////////////////////////////////////////////
			/// ///////////////////////////////////////////////////////////////////////////////
			//  Esplora
			/// ///////////////////////////////////////////////////////////////////////////////
			// ////////////////////////////////////////////////////////////////////////////////
			#pragma region ESPLORA
			#if 0

						MSG("BE>")

							TOGGLEPIN(Pin_LED_TOP_B);
						robot.status.parameters.sonarStartAngle = 0;
						robot.status.parameters.sonarEndAngle = 180;
						robot.status.parameters.sonarStepAngle = 30;
						robot.status.parameters.sonarScanSweeps = 1;
						robot.status.parameters.sonarMedianSamples = 2;
						robot.status.parameters.sonarScanSpeed = 30; // map(analogRead(Pin_AnaPot1), 0, 1023, 10, 500);  //was = 30 ms di attesa tra due posizioni

						robot.LDSScanBatch();
						alfa = 90 - robot.status.parameters.SonarMaxDistAngle;
						alfa = 30;
						MSG2("Max dist @alfa:", alfa);
						MSG2("Max dist cm:", robot.status.parameters.sonarMaxDistance)
							TOGGLEPIN(Pin_LED_TOP_B);

						// invia i dati Sonar all'Host
						//onCmdSonarSendData(&cmdMMI);
						TOGGLEPIN(Pin_LED_TOP_B);

						///osalSysDisable();
						robot.rotateDeg(alfa);
						osalSysEnable();
						cmDone = robot.moveCm(robot.status.parameters.sonarMaxDistance);	// avanti
						if (cmDone < (robot.status.parameters.sonarMaxDistance - 1))	//ostacolo ?
						{
							MSG("Obstacle!");
							robot.moveCm(-FWDIST);	// torna indietro
							TOGGLEPIN(Pin_LED_TOP_B);
							stuckCount++;
							if (stuckCount > 2)
							{
								robot.rotateDeg(180); //inverto la direzione
								TOGGLEPIN(Pin_LED_TOP_B);
								stuckCount = 0;
							}
						}
						else //nessun ostacolo, azzero il contatore
						{
							stuckCount = 0;
						}
						TOGGLEPIN(Pin_LED_TOP_B);


						sleepTime = 1500;	// Sleep for 150 milliseconds.



			#endif // 0
					//  return 0;
			#pragma endregion
			#if 0//test che funzionino i sensori e movimento in ambiente chibios

						LASER_ON


						robot.go(robot.status.cmd.commandDir, robotSpeed_e::SLOW);// non bloccante
						MSG("MOTORI ATTIVATI")
							////2)avvio la rotazione del robot
							//osalSysDisable();
							////loop
							//while (true)
							//{
							//	// leggo LDS,sonar,heading
								robot.status.posCurrent.r = compass.getBearing();
								pingCm = robot.sonarPing();
								ldsmm = LDS.readRangeSingleMillimeters();
								if (!LDS.timeoutOccurred()) {
									estimatedDistCm = ldsmm / 10;
								}
								else //uso il sonar
								{
									estimatedDistCm = pingCm;
								}
								//invio i dati
								onCmdGetPose(&cmdMMI);
								MSG3("Dist: ", estimatedDistCm, "cm")
									//MSG3("Compass: ",robot.status.posCurrent.r,"°")
									//MSG3("lds: ",ldsmm, "mm")
									//	MSG3("Ping: ",pingCm, "cm")
							//}
							//osalSysEnable();

			#endif // TEST_ALL			#pragma region ESPLORA2



				break;

		#pragma endregion

		case operatingMode_e::MODE_JOYSTICK:
			//MSG(" okei comanda col gioistic");
			sleepTime = 5000;
			break;

		default:
			MSG(" modo sconosciuto");
			sleepTime = 2000;
			break;

		}

 



		LEDTOP_G_OFF

		chThdSleepMilliseconds(sleepTime);//	chThdYield();//	

	}
}
#pragma endregion 






//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//  THREAD  N O N  A T T I V I  									/////////////
/////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#pragma region THREAD NON ATTIVI
#if 0
	//////////////////////////////////////////////////////////////////////////////////
	//  blinking LED       ///////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////
	/*
	#pragma region  Processo blinking LED
	// 64 byte stack beyond task switch and interrupt needs
	static THD_WORKING_AREA(waFlashLed, 64);
	static THD_FUNCTION(FlashLed, arg) {

	while (1) {
	// Turn LED on.
	LEDTOP_G_ON
	// Sleep for 50 milliseconds.
	chThdSleepMilliseconds(20);

	// Turn LED off.
	LEDTOP_G_OFF

	// Sleep for 150 milliseconds.
	chThdSleepMilliseconds(960);
	}
	}
	#pragma endregion
	*/


	//////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////
	// thread   SCAN 
	//////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////
static THD_WORKING_AREA(waScan, 200);
static THD_FUNCTION(thdScan, arg) {

	while (!robot.status.operatingMode == MODE_AUTONOMOUS)
	{
		// Lock movements
		//chMtxLock(&mutexMotion);

		onCmdSonarScan(&cmdMMI);

		// Unlock data access.
		//chMtxUnlock(&mutexMotion);

		chThdSleepMilliseconds(1000);

	}
}




#endif // 0
#pragma endregion


// ////////////////////////////////////////////////////////////////////////////////////////////
// OS Setup (non cambia se non si aggiungono task)
// ////////////////////////////////////////////////////////////////////////////////////////////
#pragma region [CHIBIOS RTOS]

extern unsigned int __bss_end;
extern unsigned int __heap_start;
extern void *__brkval;

uint16_t getFreeSram() {
	uint8_t newVariable;
	// heap is empty, use bss as start memory address 
	if ((uint16_t)__brkval == 0)
		return (((uint16_t)&newVariable) - ((uint16_t)&__bss_end));
	// use heap end as the start of the memory address 
	else
		return (((uint16_t)&newVariable) - ((uint16_t)__brkval));
};

void thdSetup() {
	// fill pool with msgObjArray array
	//for (size_t i = 0; i < MBOX_COUNT; i++) {
	//	chPoolFree(&memPool, &msgObjArray[i]);
	//}

	//chThdCreateStatic(waSafety, sizeof(waSafety), NORMALPRIO +10, thdSafety, NULL);
	chThdCreateStatic(waReadSensorsLR, sizeof(waReadSensorsLR), NORMALPRIO + 6, thdReadSensorsLR, NULL);
	//chThdCreateStatic(waRos, sizeof(waRos), NORMALPRIO + 6, thdRos, NULL);
	chThdCreateStatic(waReadSensorsHR, sizeof(waReadSensorsHR), NORMALPRIO + 5, thdReadSensorsHR, NULL);
	chThdCreateStatic(waBrain, sizeof(waBrain), NORMALPRIO + 5, thdBrain, NULL);
	//chThdCreateStatic(waSendStatusHR, sizeof(waSendStatusHR), NORMALPRIO +4, thdSendStatusHR, NULL);
	//chThdCreateStatic(waSendStatusLR, sizeof(waSendStatusLR), NORMALPRIO +4, thdSendStatusLR, NULL);
	//chThdCreateStatic(waPCcommands, sizeof(waPCcommands), NORMALPRIO + 3, thdPCcommands, NULL);
	//chThdCreateStatic(waMMIcommands, sizeof(waMMIcommands), NORMALPRIO + 3, thdMMIcommands, NULL);
	//chThdCreateStatic(waFlashLed, sizeof(waFlashLed), NORMALPRIO + 2, FlashLed, NULL);
	//	chThdCreateStatic(waThreadMonitor, sizeof(waThreadMonitor), NORMALPRIO + 1, ThreadMonitor, NULL);
	//	chThdCreateStatic(watestRotaryEncoder, sizeof(watestRotaryEncoder), NORMALPRIO + 2, testRotaryEncoder, NULL);
	MSG("Thread chibios avviati ..");
	while (1) {}

}

// Interrupt service routines for the right motor's quadrature encoder
volatile bool interruptFlag = false;
volatile uint32_t robotstatussensorsEncR = 0;
volatile unsigned long isrCurrCallmSec = 0;
volatile unsigned long isrLastCallmSec = 0;

#define ISR_MINMUM_INTERVAL_MSEC 25  // 30 = circa ROBOT_MOTOR_STEPS_PER_ENCODERTICK * ROBOT_MOTOR_STEPS_PER_ENCODERTICK
void ISRencoder()
{
	if (robot.status.cmd.targetEncoderThicks > 0) //faccio lavorare l'ISR solo se targetEncoderThicks> 0
	{
		isrCurrCallmSec = millis();

		if (isrCurrCallmSec > (isrLastCallmSec + ISR_MINMUM_INTERVAL_MSEC))
		{
			LEDTOP_B_ON
			isrLastCallmSec = isrCurrCallmSec;
			robot.status.sensors.EncR += 1;  
			if (robot.status.sensors.EncR > robot.status.cmd.targetEncoderThicks)
			{
				LEDTOP_G_ON
				robot.stop();
				//MOTORS_DISABLE
				robot.status.cmd.targetEncoderThicks = 0;

			}
			LEDTOP_B_OFF
		}
 
	}

}
// ########################################################################################
// ########################################################################################
//  S E T U P
// ########################################################################################
// ########################################################################################
void setup()
{
	//lampeggiaLed(Pin_LED_TOP_R, 3, 5);
	//lampeggiaLed(Pin_LED_TOP_G, 3, 5);
	//lampeggiaLed(Pin_LED_TOP_B, 3, 5);
	LEDTOP_R_ON	// Indica inizio SETUP Phase
	MOTORS_DISABLE
	attachInterrupt(digitalPinToInterrupt(3), ISRencoder, CHANGE);

	pinMode(Pin_LED_TOP_G, OUTPUT);
	InitTimersSafe(); //initialize all timers except for 0, to save time keeping functions
					  //sets the frequency for the specified pin
	//bool success = SetPinFrequencySafe(Pin_LED_TOP_R, 2);

	////if the pin frequency was set successfully, turn pin 13 on
	//if (success) {
	//	digitalWrite(Pin_LED_TOP_B, HIGH);
	//	pwmWrite(Pin_LED_TOP_R, 128); //set 128 to obtain 50%duty cyle
	//	SetPinFrequency(Pin_LED_TOP_R, 2); //setting the frequency to 10 Hz
	//}
	
	SERIAL_MSG.begin(SERIAL_MSG_BAUD_RATE);
	SERIAL_MMI.begin(SERIAL_MMI_BAUD_RATE);
	SERIAL_GPS.begin(SERIAL_GPS_BAUD_RATE);
	MSG("####[  ROBOTCORE v0.3  ]####");
	WEBCAM_OFF
	WEBCAM_ON
	MSG3("Bat : ",robot.readBattChargeLevel(),"%");

	MSG2("A0 : ",analogRead(A0));
	MSG2("A1 : ",analogRead(A1));
	MSG2("A1 : ",analogRead(A2));

	robot.initHW(); //disabilita i motori
//	tone(Pin_LED_TOP_R, 2, 0);
	countDown(5);  //PER DARE IL TEMPO ALL COMPASS DI RISPONDERE
	Wire.begin(); // default timeout 1000ms

#if OPT_SEROVOSONAR
	robot.initRobot( &servoSonar);
#endif
#if OPT_GPS
	robot.initRobot( &Gps);
#endif
#if OPT_LDS
	robot.initRobot( &LDS);
	robot.initLDS( &LDS);
#endif
#if OPT_COMPASS
	robot.initRobot( &compass);
	robot.initCompass(&compass);//include compass.begin();
#endif
#if OPT_STEPPERLDS


	robot.initRobot( &myLDSstepper);
#endif
#if OPT_MPU6050

	MSG("Initializing MPU6050...");

	mpu.initialize();
	// load and configure the DMP
	devStatus = mpu.dmpInitialize();
	// supply your own gyro offsets here, scaled for min sensitivity
	mpu.setXGyroOffset(220);
	mpu.setYGyroOffset(76);
	mpu.setZGyroOffset(-85);
	mpu.setZAccelOffset(1788); // 1688 factory default for my test chip
	MSG("...done");

 #endif

 
	robot.initRobot();

//	noTone(Pin_LED_TOP_R);


	MSG("ACCENDI I MOTORI");
//	LEDTOP_G_ON	// Indica inizio SETUP Phase


// test di obstacleFree()		 
#if 0		
	MSG("MOTION TEST");
	countDown(5);  //PER DARE IL TEMPO ALL COMPASS DI RISPONDERE

	MSG("MOVE FW 5cm");
	robot.moveCm(10);
	robot.moveCm(-10);
	robot.rotateDeg(45);
	robot.rotateDeg(-90);
	robot.rotateDeg(45);
	MSG("END Testing robot.moveCm()");
#endif // 1


 




	//	robot.compassCalibrate();

	LEDTOP_R_ON
	

	#define STARTING_HEADING 90
	MSG2("## goHeading :", STARTING_HEADING);
	robot.stop();
	//MOTORS_DISABLE
	robot.status.sensors.EncR = 0;//resetto il conteggio encoder ticks
	robot.status.cmd.targetEncoderThicks = 0;
	robot.goHeading(STARTING_HEADING, 5);
	LEDTOP_R_OFF

#if 0
		for (size_t i = 0; i < 5; i++)
		{
			MSG2("LDS cm: ", robot.getLDSDistanceCm());
			MSG2("         alfa: ", robot.readCompassDeg());
			delay(500);
		}

	delay(5000);

#endif // 0

	// ########################################################################################
	// ########################################################################################
	//  TEST OPZIONALI
	// ########################################################################################
	// ########################################################################################

	#pragma region TEST OPZIONALI PRIMA DELL'AVVIO DELL'OS
			int targetAngle = 0;
			int freeDistCm = 0;
			char charVal[10];

		// test IMU
		#if 0

			MSG("TST MPU")
			long tStart = millis();
			while ( millis()- tStart < 10000){
				if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
					// reset so we can continue cleanly
					mpu.resetFIFO();
					MSG("FIFO overflow!");

					// otherwise, check for DMP data ready interrupt (this should happen frequently)
				}
				else if (mpuIntStatus & 0x02) {
					// wait for correct available data length, should be a VERY short wait
					while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

					// read a packet from FIFO
					mpu.getFIFOBytes(fifoBuffer, packetSize);

					// track FIFO count here in case there is > 1 packet available
					// (this lets us immediately read more without waiting for an interrupt)
					fifoCount -= packetSize;

					mpu.dmpGetQuaternion(&q, fifoBuffer);
					dtostrf(q.z, 4, 3, charVal);	//float to char array
					MSG("MPU")
					Serial1.print("1,quat ");
					Serial1.print(q.w);
					Serial1.print(",");
					Serial1.print(q.x);
					Serial1.print(",");
					Serial1.print(q.y);
					Serial1.print(",");
					Serial1.print (q.z);
					Serial1.println(";");

				}

				robot.readBumpers();

			}
				
		#endif // 1

		// test LDS
		#if 0
			while (true){
				angle = robot.goCWAndScanDeg(360);
				delay(1000);
				robot.goHeading(angle,3);



				delay(5000);

			}
				
		#endif // 1
		#if 0
			while (true){
				MSG2("Angle with max FreeDist: ",robot.goCWAndScanDeg(360 ) );
					delay(2000);
				MSG2("Angle with max FreeDist: ",robot.goCWAndScanDeg(-360) );
					delay(2000);

			}
				
		#endif // 1


		// test ESPLORA
		#if 0
			int backDist = 10; // quanto va indietro quando trova ostacolo
			int movedmm = 0;
			int minFreeDistCm = 10;
			int nextRotCw = -1;
			robot.status.sensors.ignoreIR = true; //usato da isObstacleFreeIR()
			while (true)
			{
				LEDTOP_B_ON
				nextRotCw *= -1; //inverto il senso di rotazione
				// cerco l'angolo con il maggior perccorso libero
				targetAngle =robot.goCWAndScanDeg(nextRotCw*180);
				delay(1000); //per debug

				MSG("----------");
				MSG2("FreeDist : ", freeDistCm);
				MSG2("  AT Alfa:", targetAngle);
				MSG("----------");
				LEDTOP_B_OFF

				LEDTOP_G_ON
				// ruoto fino all'angolo
				robot.goHeading(targetAngle);//robot.rotateDeg(deltaAngle);
				MSG2("FreeDist: ", freeDistCm);
				delay(1000); //per debug
				//// mi metto in moto
				//robot.go(commandDir_e::GOF, robotSpeed_e::MEDIUM);
				//freeDistCm = robot.status.sensors.ldsEchos_cm[targetAngle];
				////
				//while (robot.isObstacleFreeBumpers() && (freeDistCm>5) )
				//{
				//	freeDistCm= robot.getLDSDistanceCm();
				//	robot.readBumpers();

				//}

				LEDTOP_G_OFF

					////avanti è abbastanza libero ?
				movedmm = robot.moveCm(freeDistCm );
				MSG3("Moved: ", movedmm, "mm");

					//ho incontrato ostacolo?
					if ((movedmm < 100)||(movedmm < 10*(freeDistCm - backDist)))
					{
						//sì torno indietro

						robot.moveCm(-backDist);
						LEDTOP_R_ON

						if ((robot.status.sensors.bumper.left)||(robot.status.sensors.irproxy.fl)) //ostacolo a sinistra
						{

							robot.goCWUntilMinFreeDist(50);
							//angle = robot.goCWAndScan(10, true);
							//robot.goHeading(angle);

						}
						else if ((robot.status.sensors.bumper.right) || (robot.status.sensors.irproxy.fr))
						{
							robot.goCWUntilMinFreeDist(-50);
							//angle = robot.goCWAndScan(10, false);
							//robot.goHeading(angle);

						}
						//robot.rotateDeg(nextRotCw*20);
						robot.moveCm(robot.getLDSDistanceCm() - backDist);
						//minFreeDistCm -= max(minFreeDistCm-20,20);


						LEDTOP_R_OFF

					}
					else //no ostacolo
					{
						//minFreeDistCm = min(200, freeDistCm + 20);
						//LEDTOP_B_ON

					}
				//}
				//else 
				//{
				//	// no
				//	robot.moveCm(-backDist);
				//	//robot.goCWUntilMinFreeDist(minFreeDistCm);
				//	//robot.rotateDeg(nextRotCw*20);
				//	// path libero > incremento

				//}

			}
		#endif // 1


		//TEST_COMPASS_3   ### BLOCCANTE ###
		#if 1		//calibra  e poi ruota verso Nord
			// Initialise the sensor 
			robot.stop();
			robot.status.sensors.EncR = 0;


			MSG("Test GO 1 giro....")
			//robot.goCWAndScan(20);
			long targetRads = 2 * PI; //un giro
			robot.status.cmd.targetEncoderThicks = targetRads *  ROBOT_MOTOR_STEPS_PER_RADIANT /  ROBOT_MOTOR_STEPS_PER_ENCODERTICK;
			MSG2("Target Enc.", robot.status.cmd.targetEncoderThicks);

			robot.go(commandDir_e::GOR, robotSpeed_e::MEDIUM);
			countDown(5);

			while (true)
			{
				MSG2("Enc R.", robot.status.sensors.EncR);
				MSG2("LDS.", robot.getLDSDistanceCm());
				delay(1000);

			}

/*
			long targetRads = 2 * PI; //un giro
			robot.status.cmd.targetEncoderThicks = targetRads *  ROBOT_MOTOR_STEPS_PER_RADIANT /  ROBOT_MOTOR_STEPS_PER_ENCODERTICK;
			MSG2("Target Enc.", targetEncoderTicks);

			// per il tempo a disposizione...
			while (robot.status.sensors.EncR < targetEncoderTicks) //tempo sufficiente per fare almeno un giro
			{
				// rilevo distanza e angolo
				MSG2("Enc R.", robot.status.sensors.EncR);
				delay(500);
			}
			
			robot.stop();

			MSG("..End Scan");
*/
		#endif // COMPASS



		//TEST_COMPASS_2   ### BLOCCANTE ###
		#if 0		//calibra  e poi ruota verso Nord
			// Initialise the sensor 
				robot.stop();
			MSG("CALIBRAZIONE COMPASS. Accendere i motori......")
			countDown(5);
			robot.compassCalibrate();

			// al termine della calibrazione faccio puntare il robot ai 4 punti cardinali
			MSG("Ora punto a nord DOPO CALIBRAZIONE ...")
			robot.goHeading(0,3);
			MSG("**********Arrivato********");
			MSG3("Bearing di arrivo: ", compass.getBearing(), "°");

			while (true) {
				TOGGLEPIN(Pin_LED_TOP_G);
				delay(300);
			}
		#endif // COMPASS

		// TEST SONAR
		#if 0	
			MSG("SONAR ONLY TEST..");
			for (size_t i = 0; i < 5; i++)		//		while (true)
			{
				TOGGLEPIN(Pin_LaserOn);
				MSG3("Ping: ", robot.sonarPing(), "cm");
				delay(300);
			}
		#endif // 0

		// ruota finchè non trova davanti una distanza minima libera
		#if 0 		
			MSG("goCWUntilMinFreeDist..")
			robot.status.sensors.sonarEchos[90] = robot.goCWUntilMinFreeDist(200);
			MSG2("dist found cm:", robot.status.sensors.sonarEchos[90]);
		#endif

		// RUOTA con velocità crescente 1 secondo ogni step
		#if 1
		
			// Attenzione !! Se presente un altro tone() non funziona
			robot.stop();
			MSG("TEST robot.go()..")
			MSG("PLEASE SWITCH MOTORS ON")
			MSG("DISCONNECT CABLES")
			MSG("Start test in 10 secondi...")
			countDown(10);
			int ck = ROBOT_MOTOR_CLOCK_microsecondMAX;
			bool cont = true;
			while (robot.isObstacleFreeBumpers())
			{
				MSG2("Current ck:", ck)
				robot.go(commandDir_e::GOR, ck);
				delay(1000);
				ck -= 200;
				if (ck < ROBOT_MOTOR_CLOCK_microsecondMIN)
				{
					//torno indietro per 3 secondi a velocità media
					robot.stop();
					robot.go(commandDir_e::GOL, robotSpeed_e::MEDIUM);
					delay(3000);
					cont = false;
				}
				robot.readBumpers();
			}

			MSG("Fine test......");
			robot.stop();
			//while (true) {}

		#endif // 0

 
		// I2C Scanner	 
		#if 0		


			byte error, address;
			int nDevices;

			Serial.println("Scanning...");

			nDevices = 0;
			for (address = 1; address < 127; address++)
			{
				// The i2c_scanner uses the return value of
				// the Write.endTransmisstion to see if
				// a device did acknowledge to the address.
				error = I2c.write(address, 0);


				if (error == 0)
				{
					Serial.print("I2C device found at address 0x");
					if (address < 16)
						Serial.print("0");
					Serial.print(address, HEX);
					Serial.println("  !");

					nDevices++;
				}
				else if (error == 4)
				{
					Serial.print("Unknow error at address 0x");
					if (address < 16)
						Serial.print("0");
					Serial.println(address, HEX);
				}
			}
			if (nDevices == 0)
				Serial.println("No I2C devices found\n");
			else
				Serial.println("done\n");

			delay(5000);           // wait 5 seconds for next scan

		#endif 

		// TEST_GPS 
		#if 0
		//CHIAVARI LAT	44.326953 LONG 9.289679

		#endif 


		// TEST_SPEECH
		#if 0		 
			SERIAL_MSG.println(F("1,test Speak;"));
			SERIAL_MMI.println(F("28,CIAO CIAO;"));
		#endif // TEST_SPEECH


		//test ixproxy
		#if 0	
			SERIAL_MSG.print(F("1,testing ixproxy...;"));
			while (true)
			{
				robot.readSensors();
				if (robot.status.sensors.irproxy.fw != robot.statusOld.sensors.irproxy.fw)
				{
					SERIAL_MSG.println(F("1,irproxy.fw changed;"));
					TOGGLEPIN(Pin_LED_TOP_B);

				}
				else if (robot.status.sensors.irproxy.fwHL != robot.statusOld.sensors.irproxy.fwHL)
				{
					SERIAL_MSG.println(F("1,irproxy.fwHl changed;"));
					TOGGLEPIN(Pin_LED_TOP_G);

				}
			}

		#endif // 0//test ixproxy

 


		// TEST_COMPASS_1 SENZA CALIBRAZIONE 			
		#if 0		///Ruota misurando in continuazione
			MSG("Ora punto a nord SENZA CALIBRAZIONE...")
			robot.goHeading(3);
		#endif // 1




		// RUOTA E MISURA LDS,COMPASS,SONAR.. 
		#if 0  

			MSG("TEST LDS,COMPASS,SONAR..");
			MSG("ACCENDI MOTORI E SCOLLEGA CAVI");

			countDown(5);
			LASER_ON

			//1)inizializzo i sensori e le variabili
			int pingCm = 0;
			int ldsmm = 0;
			int estimatedDistCm = 0;

			//MSG("Target 180");
			//delay(1000);
			//robot.rotateHeading(180, 5);
			//MSG("Target 90");
			//delay(1000);
			//robot.rotateHeading(90, 5);
			//delay(1000);
			//MSG("Target 270");
			//robot.rotateHeading(270, 5);
			//delay(1000);
			//MSG("Target 0");
			//robot.rotateHeading(270, 5);
			//MSG("Target 180");


		#endif // TEST_ALL

		//PROVA SERVO----------------------------------
		#if 0	

			MSG("Servo al centro...")
			//servoSonar.attach(Pin_ServoSonarPWM);
			robot.ServoAtPos(10); delay(1000);
			robot.ServoAtPos(160); delay(1000);
			robot.ServoAtPos(90); delay(1000);


		#endif // 0

	#pragma endregion //regione dei test



	//SERIAL_MSG.println(F("1,running IBIT...;"));
	//robot.runIBIT(300);



	MSG("GO HEADING 330°");
	// mi allineo parallelo al corridoio tra libreria e divano
	robot.goHeading(330, 5); //bloccante se non va il Compass

	LEDTOP_R_OFF
	lampeggiaLed(Pin_LED_TOP_G, 5, 10);

	chBegin(thdSetup);
}


void loop() {
	// not used
}

#pragma endregion


