//--------------------------------------------------------------------------------------------------
// Name: Chanpreet Singh and Gurbani Baweja 
// 
// CMPUT 275 - Tangible Compting - Winter 2019
// Final Project :- Morse code communication uing Diffi Helman scheme
//--------------------------------------------------------------------------------------------------


#include <Arduino.h>
// core graphics library (written by Adafruit)
#include <Adafruit_GFX.h>
// Hardware-specific graphics library for MCU Friend 3.5" TFT LCD shield
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>


MCUFRIEND_kbv tft;

// touch screen pins, obtained from the documentaion
#define YP A3 // must be an analog pin, use "An" notation!
#define XM A2 // must be an analog pin, use "An" notation!
#define YM 9  // can be a digital pin
#define XP 8  // can be a digital pin
// physical dimensions of the tft display (# of pixels)
#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320
// calibration data for the touch screen, obtained from documentation
// the minimum/maximum possible readings from the touch point
#define TS_MINX 100
#define TS_MINY 120
#define TS_MAXX 940
#define TS_MAXY 920
#define RED 0xF800

// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

//font 
#include <Fonts/FreeMonoBoldOblique12pt7b.h>
const int buzzer=35; //buzzer pin -orange
const int click=41; //button-blue
const int send_button=29; //button-red
int Pin = 25; // to know if arduino is server or client
String message_string_from_morse=""; // this is sring that contain decoded morse code message 
String decrypted = ""; //this string have message that is recieved and decoded after using decryption scheme

 //-------------------------------------------------------------------------------------

void setup() {
  init();
  Serial.begin(9600);
  Serial3.begin(9600);
  pinMode(Pin, INPUT);
 
  // tft display initialization
  uint16_t ID = tft.readID();
  tft.begin(ID);

  pinMode(buzzer, OUTPUT);
  pinMode(click, INPUT_PULLUP);
  pinMode(send_button, INPUT_PULLUP);

  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(5);
}

//--------------------------------------------------------------------------------

// controls the overall state during key exchange
enum ProgramStates { 
    KEY_EXCHANGE, MESSAGE_ENTER, ENCRYPTION, DECRYPTION, MESSAGE_DISPLAY
};

ProgramStates curr_mode = KEY_EXCHANGE;

enum ExchangeStates { START,
    GenerateKeys, Listen, WaitingForKey, WaitForAck, DataExchange, Start, WaitingForAck, SendKey
};

//----------------------------------------------------------------------------------------------

// send a uint32t value to Serial3
// Citation: Funcition fron  CMPUT 274
void uint32_to_serial3(uint32_t num) {
    Serial3.write((char) (num >> 0));
    Serial3.write((char) (num >> 8));
    Serial3.write((char) (num >> 16));
    Serial3.write((char) (num >> 24));
}

// read a uint32t value from Serial3
// Citation: Funcition fron  CMPUT 274
uint32_t uint32_from_serial3() {
    uint32_t num = 0;
    num = num | ((uint32_t) Serial3.read()) << 0;
    num = num | ((uint32_t) Serial3.read()) << 8;
    num = num | ((uint32_t) Serial3.read()) << 16;
    num = num | ((uint32_t) Serial3.read()) << 24;
    return num;
}

//-----------------------------------------------------------------------------------------------

//Generate random n_bit bit number
// Citation: Funcition fron  CMPUT 274
uint32_t randnum(uint32_t n_bit) {

    uint32_t bit = 0,result = 0;

    for(uint32_t i = 0; i < n_bit; i++){
        bit=analogRead(1);
        result=(result<<1);
        result= (result|(bit&1));
        delay(5);

    }
    result=result+((uint32_t)1<<(n_bit));
    return (result);
}

//-----------------------------------------------------------------------------------------------------

// Test whether a number is prime
// Citation: Funcition fron  CMPUT 274
uint32_t primetest(uint32_t p){

    for(uint32_t i=2;i<sqrt(p);i++){
        if((p%i)==0){
            return 0;
        }
    }
    return 1;//if p is prime
}
//-----------------------------------------------------------------------------------

// Citation: Funcition fron  CMPUT 274
uint32_t mod_multiply(uint32_t a, uint32_t b, uint32_t mod) {
    //Perform (a*b) mod m, carefully for both a and b 31-bit numbers in a 32-bit tpye.
    uint32_t pow = b % mod;
    a = a % mod;
    uint32_t ans = 0;
    while (a > 0) {
        if (a & 1 == 1) {
            //Executed whenever LSB is 1 only
            ans = (ans + pow) % mod;
        }

        pow = (pow * 2) % mod;
        a >>= 1;
    }
    return ans;
}
//---------------------------------------------------------------------------------

// Determin whether there are enough bytes waiting in the serial port
// Citation: Funcition fron  CMPUT 274
bool wait_on_serial3 (uint8_t nbytes, long timeout) {
    unsigned long deadline = millis() + timeout;
    while (Serial3.available() < nbytes && (timeout < 0 || millis() < deadline)) {
        delay(1);
    }
    return Serial3.available() >= nbytes;
}

//-------------------------------------------------------------------------------------------

// Exchange process for server
uint32_t server_exchange(uint32_t p, uint32_t g, uint32_t a, uint32_t& encryption_key) {

    ExchangeStates server_state = GenerateKeys; // Don't delete this!
    uint32_t CR, ACK;
    uint32_t ckey, skey = 1, index;
    while (server_state != DataExchange) {
        if (server_state == GenerateKeys) {
            // generate server public key, which equals g^a mod p
            uint32_t pow = g;
            index = a;
            while(index > 0) {
                if ( index & 1 == 1) {
                    skey = mod_multiply(skey, pow, p);
                }
                pow = mod_multiply(pow, pow, p);
                index >>= 1;
            }
            server_state = SendKey;
        }
        if(server_state == SendKey) {
            // send all three keys to the client: p, g and server public key
            uint32_to_serial3(p);
            uint32_to_serial3(g);
            uint32_to_serial3(skey);
             //Serial.println("keys sent");
            server_state = WaitForAck;
        }
        if(server_state == WaitForAck) { // wait for the client to acknowledge
            if (wait_on_serial3(1, 1000)) {
                ACK = Serial3.read();
                if (ACK == 65) {
                    server_state = Listen; // if the client successfully receives all the keys, server will go to the next phase
                }
            }
        }
        if (server_state == Listen) { // determin whether the server is ready to receive, if no valid CR is received, the server will stay
                                           // in this state
            if (Serial3.available() > 0) { 
                CR = Serial3.read();
            }
            if (CR == 67) server_state = WaitingForKey;
        }
        if(server_state == WaitingForKey) {
            if (wait_on_serial3(4, 1000)) {  // read client's public key from serial port
                ckey = uint32_from_serial3();
                Serial3.write('A'); // give acknowledgement

                // calculate the final encryption key, which is (g^b)^a mod p
                uint32_t pow = ckey;
                encryption_key = 1;
                index = a;
                while(index > 0) {
                    if ( index & 1 == 1) {
                        encryption_key = mod_multiply(encryption_key, pow, p);
                    }
                pow = mod_multiply(pow, pow, p);
                index >>= 1;
                }
                server_state = DataExchange; // finish the key exchange process
            }
        }
    }

}
//---------------------------------------------------------------------------------------
uint32_t client_exchange(uint32_t& p, uint32_t& g, uint32_t b, uint32_t& encryption_key) {

    ExchangeStates client_state = Listen;
    uint32_t ckey = 1, skey, index;
    uint32_t CR, ACK;
    while (client_state != DataExchange) {
        if (client_state == Listen) {  //read all 3 keys from serial port: p, g, server's public key
            if (wait_on_serial3(12, 1000)) {
                p = uint32_from_serial3();
                g = uint32_from_serial3();
                skey = uint32_from_serial3();
                Serial3.write('A');  // If all 3 keys are successfully received, give acknowledgement
                client_state = GenerateKeys;  // go to the next stage 
            }
        }
        if (client_state == GenerateKeys) {
            // generate client's public key, which is g^b mod m
            uint32_t pow = g;
            index = b;
            while(index > 0) {
                if ( index & 1 == 1) {
                    ckey = mod_multiply(ckey, pow, p);
                }
                pow = mod_multiply(pow, pow, p);
                index >>= 1;
            }
            client_state = SendKey;
        }
        if (client_state == SendKey) {
            Serial3.write('C'); // first give CR to the server
            Serial3.flush();
            uint32_to_serial3(ckey); // send client's public key to the server

            client_state = WaitingForAck;
        }
        if (client_state == WaitingForAck) {
            if (wait_on_serial3(1, 1000)) {
                ACK = Serial3.read(); // determin whether server successfully acknowledges
                if (ACK == 65) {  // if yes, compute the final encryption key, which is (g^a)^b mod m
                    uint32_t pow = skey;
                    index = b;
                    encryption_key = 1;
                    while(index > 0) {
                        if ( index & 1 == 1) {
                            encryption_key = mod_multiply(encryption_key, pow, p);
                        }
                        pow = mod_multiply(pow, pow, p);
                        index >>= 1;
                    }
                    client_state = DataExchange;
                }
            }
        }
    }

}
//--------------------------------------------------------------------------------------------
uint64_t encrypt(uint32_t original_info, uint32_t encryption_key) {
    // encrypt the original message by adding the original information by the encryption key
    uint64_t encrypted_info;
    encrypted_info = original_info + encryption_key;
    return encrypted_info;
}

//---------------------------------------------------------------------------------------------
uint64_t decrypt(uint64_t encrypted_info, uint32_t encryption_key) {
    // calculate the original info
    uint64_t decrypted_info;
    decrypted_info = encrypted_info - encryption_key;
    return decrypted_info;
}

//---------------------------------------------------------------------------------------------

// to store the dot and slash patern and associated word with it
struct library {
  String code, decode;
  library(int lon1 = 0, int lat1 = 0) : decode(lon1), code(lat1) {}  //CCitation: Stack Overflow
};

//------------------------------------------------------------------------------------------------

//This function is used to return code or decode as per mode
String process_code(int mode, String code, String decode){

	library database[37];

	//not a big size so we make it each time but for effiecenct we could make it global
	database[0].decode="A";
	database[0].code=".-";
	database[1].decode="B";
	database[1].code="-...";
	database[2].decode="C";
	database[2].code="-.-.";
	database[3].decode="D";
	database[3].code="-..";
	database[4].decode="E";
	database[4].code=".";
	database[5].decode="F";
	database[5].code="..-.";
	database[6].decode="G";
	database[6].code="--.";
	database[7].decode="H";
	database[7].code="....";
	database[8].decode="I";
	database[8].code="..";
	database[9].decode="J";
	database[9].code=".---";
	database[10].decode="K";
	database[10].code="-.-";
	database[11].decode="L";
	database[11].code=".-..";
	database[12].decode="M";
	database[12].code="--";
	database[13].decode="N";
	database[13].code="-.";
	database[14].decode="O";
	database[14].code="---";
	database[15].decode="P";
	database[15].code=".--.";
	database[16].decode="Q";
	database[16].code="--.-";
	database[17].decode="R";
	database[17].code=".-.";
	database[18].decode="S";
	database[18].code="...";
	database[19].decode="T";
	database[19].code="-";
	database[20].decode="U";
	database[20].code="..-";
	database[21].decode="V";
	database[21].code="...-";
	database[22].decode="W";
	database[22].code=".--";
	database[23].decode="X";
	database[23].code="-..-";
	database[24].decode="Y";
	database[24].code="-.--";
	database[25].decode="Z";
	database[25].code="--..";
	database[26].decode="0";
	database[26].code="-----";
	database[27].decode="1";
	database[27].code=".----";
	database[28].decode="2";
	database[28].code="..---";
	database[29].decode="3";
	database[29].code="...--";
	database[30].decode="4";
	database[30].code="....-";
	database[31].decode="5";
	database[31].code=".....";
	database[32].decode="6";
	database[32].code="-....";
	database[33].decode="7";
	database[33].code="--...";
	database[34].decode="8";
	database[34].code="---..";
	database[35].decode="9";
	database[35].code="----.";
	database[36].decode="END";
	database[36].code="END";

	//code to decoded alphabets
	//if mode is 1 then we want to know alphabet
	if(mode==1){
		int i=0;
		while(true){
			if (database[i].code == code){
				return database[i].decode;
			}
			else if (database[i].decode == "END"){
				Serial.println("Error in finding match");  // You output this to the serial mon?
				return "11111";
			}
			i++; // First, this loop doesn't have a ending situation, so it's infinite
	         // Second, if you increase i in this way, this will cause a segmentation fault
		}
	}

	//alphabet to morse code
	//if mode is 2 then we want to know the morse code for each alphabet
	if(mode==2){
		int i=0;
		while(true){
			if (database[i].decode == decode){
				return database[i].code;
			}
			i++; 
		}
	}
}

//-------------------------------------------------------------------------------------------------

//This function is used to return the duration for which button was pressed
int process_click(){
	long long start=0,end=0,time=0;
	//program come here and stop untill morse button is pressed it does nothing.
	while(digitalRead(click)==HIGH ){
		//while waiting for morse button if we press send button then it return 222 is signal that send is pressed.
		if(digitalRead(send_button)==LOW){
			while(digitalRead(send_button)==LOW){};
			return 2222;
		}
	}
	tone(buzzer,1000);
	start=millis();
	//to know how long morse button was low
	while(digitalRead(click)==LOW){};
	noTone(buzzer);
	end=millis();
	time=end-start;
	return (time);
}

//------------------------------------------------------------------------------------------------


//This funciton is used to process if the user mean dot or the dash and accordingly make the message.
void input_mode(){
	Serial.println("--------------------");
	Serial.println("Enter Message: ");
	tft.fillScreen(TFT_BLACK);
	tft.drawRect(5,5,475,315,TFT_GREEN);
	tft.setCursor(10,10);
  	tft.println("Please enter the message: ");
	String code="";
    
    while(true && curr_mode == MESSAGE_ENTER){
	  	int click_time = process_click();
	  	if(click_time > 20 && click_time < 200){
	  		code+=".";
	  	}
	  	else if(click_time > 200 && click_time < 600){
	  		code+="-";
	  	}
	  	else if(click_time > 600 && click_time < 1000){
	  		//this time duration mean used have finished entering a character
	  		String interm = process_code(1,code," ");
	  		if(interm!="11111"){
	  			//if interm is 11111 then it mean user entered wrong morse code
		  		message_string_from_morse += interm;
		  		
		  		code="";
		  		Serial.println(message_string_from_morse);

		  		tft.fillRect(10,30,100,100,TFT_BLACK);
		  		tft.setCursor(10,50);
		  		tft.print(message_string_from_morse);
	  		}
	  		code="";
	  	}
	  	//this mean used want to add a space
	  	else if(click_time>1000){
	  		code="";
	  		message_string_from_morse+=" ";
	  	}

	  	//if 2222 is returned by processclick() then it mean send is pressed so we change the mode.
	  	if(click_time==2222){
	  		curr_mode = ENCRYPTION;
	  		break;
	  	}
 
 		tft.fillRect(10,275,100,25,TFT_BLACK);
 		tft.setCursor(10,275);
 		tft.println(code);
	    Serial.print(click_time);	
	    Serial.print("  ");
	    Serial.println(code);
  	}
  	//added to know when string ends
  	message_string_from_morse += "!";
}

//-----------------------------------------------------------------------------------------------


//This function is used to play the morse code sound when message is rescieved
void play_message(){

	Serial.print("Message Rescieved - ");
	Serial.println(decrypted);
	tft.fillScreen(TFT_BLACK);
	tft.drawRect(5,5,475,315,TFT_GREEN);
	tft.setTextSize(3);
	tft.setCursor(10,10);
  	tft.println("Message Rescieved");
  	String printer="";
	decrypted+="!";
	int i=0;
	//this loop is used to keep playing sound for all chracters in the message
	while(decrypted[i]!='!'){
		String deco = String(decrypted[i]);
		String code = process_code(2," ",deco);
		printer+=deco;
		code+="!";
		int n=0;
		//this loop is used to play sound for all dots and dashes for a character
		while (true){
			if(code[n]=='.'){
				tone(buzzer,1000);
				delay(200);
				noTone(buzzer);
			}
			else if(code[n]=='-'){
				tone(buzzer,1000);
				delay(600);
				noTone(buzzer);
			}
			else{
				delay(600);
				break;
			}
			n++;
			delay(200);
		}
		tft.setCursor(10,100);
  		tft.println(printer);
		i++;
		delay(600);
	}
}

//-------------------------------------------------------------------------------------------

//This function have graphical detail for program to start.
void welcome(){
  tft.setCursor(100,100);
  tft.setCursor(125,150);
  tft.setFont(&FreeMonoBoldOblique12pt7b);
  tft.setTextSize(2);
  tft.print("Welcome");
  delay(3000);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(100,100);
  tft.print("Morse Code");
  tft.setCursor(50,200);
  tft.println("Communication");
  tft.setTextSize(1);
  tft.println("       Using Diffie Hellman");
  delay(5000);
  tft.setFont();
  tft.setCursor(0,0);
  tft.setTextSize(3);
  tft.setRotation(1);
}
//This function is used for decryption screen graphics
void decryption_graphics(){
	Serial.println("DECRYTION Mode - Waiting for Message.");
    tft.fillScreen(TFT_BLACK);
	tft.drawRect(5,5,475,315,TFT_RED);
	tft.setTextColor(TFT_RED);
	tft.setCursor(10,150);
	tft.setTextSize(2);
	tft.setFont(&FreeMonoBoldOblique12pt7b);
	tft.print("   Waiting for");
	tft.setCursor(10,200);
	tft.print("     Message");
	tft.setFont();
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(3);
}

//---------------------------------------------------------------------------------------------

//This function is used to handle the Diffi Helman Key exchange mechanism
void keyExchange(uint32_t& encryption_key, uint64_t& encrypted_info, uint64_t& decrypted_info){
	uint32_t p, g, a, b;
	if (digitalRead(Pin) == HIGH){
	    Serial.println("I am server!");
	    a = randnum(15); // generate random 15 bit integer
	    p = randnum(15); // generate random 15 bit prime integer
        g = randnum(15); // generate random 15 bit generator
	    while(primetest(p) != 1) {
	        p = randnum(15);
	    }
	    server_exchange(p, g, a, encryption_key);
	    curr_mode = MESSAGE_ENTER; //make sender ready to send
	    Serial.print("encryption key - ");
	    Serial.println(encryption_key);
	}
	// If pin 25 is connected to GND then arduino act as a client.
	else if (digitalRead(Pin) == LOW){
	    Serial.println("I am client!");
	    b = randnum(15);// generate random 15 bit integer
	    client_exchange(p, g, b, encryption_key);
	    curr_mode = DECRYPTION;// maker resciever read
	    Serial.print("encryption key - ");
	    Serial.println(encryption_key);
	    Serial3.flush();  
	}
	delay(25);
	Serial.end();
	Serial3.end();
}

//--------------------------------------------------------------------------------------------

int main(){
    setup();

    uint32_t original_info,encryption_key=0;
    uint64_t encrypted_info, decrypted_info;

    // Before actual communication, we exchange keys
    if(curr_mode == KEY_EXCHANGE){
	    keyExchange(encryption_key,encrypted_info,decrypted_info);
	}

	Serial3.flush();
	setup();
	welcome();
	int num_de=0; 
	Serial.println("Ready to communicate:");

	//Now keys have been set and we are ready
    while(true){
	    if(curr_mode == MESSAGE_ENTER){
	    	input_mode();
	    }
	    if(curr_mode == ENCRYPTION){
	    	int counter = 0;
	    	//this loop send all the charaters in the message untill ! is reached
	    	while(message_string_from_morse[counter]!='!'){
          		original_info = message_string_from_morse[counter];
	    		encrypted_info = encrypt(original_info, encryption_key);
	    		uint32_to_serial3(encrypted_info);
	    		delay(50);
          		counter++;
	    	}
 			//we add ! again for decoder to know when whole message has been decoded.
 			encrypted_info = encrypt('!', encryption_key);
	    	uint32_to_serial3(encrypted_info);
	    	curr_mode=DECRYPTION;
	    }
	    if(curr_mode == DECRYPTION){
        	
        	//we dont want to keep printing on the screen
	    	if(num_de==0){
	    		decryption_graphics();
	    		num_de=1;
  			}
	    	if (Serial3.available() > 3){ 
        		encrypted_info = uint32_from_serial3();
        		decrypted_info = decrypt(encrypted_info, encryption_key);
	            char ans=char(decrypted_info); 
	            //when ! is decoded then we move to displaying the message and stop decoding
        		if(ans=='!'){
        			curr_mode = MESSAGE_DISPLAY;
        		}
            	else{
        			decrypted += ans;
        		}
    		}
	    }
	    if(curr_mode == MESSAGE_DISPLAY){
  			num_de=0;
	    	play_message();
	    	//reset everything to enter message state.
	    	decrypted="";
	    	message_string_from_morse="";
	    	curr_mode = MESSAGE_ENTER;
	    }
	}
	Serial.flush();
  	return 1;  
  }
