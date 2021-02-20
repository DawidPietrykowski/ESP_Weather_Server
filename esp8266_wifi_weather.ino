#include <ESP8266WiFi.h>
#include <DHT.h>
#include <LittleFS.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <algorithm>

#define DHTPIN 2
#define DHTTYPE DHT11
#define BUFFER_SIZE 16384
#define DHT_SAMPLE_INT 4000
#define DHT_WRITE_INT 300000
#define PAGE_FILE_NAME "index.html"
#define MAX_FILE_SIZE 236004
#define UDP_TIMEOUT 500
#define UDP_RETRY 4
#define HTTP_TIMEOUT 4000
#define WOL_LISTEN_PORT 49500

#define SAMPLE_COUNT (DHT_WRITE_INT/DHT_SAMPLE_INT)

#define DEBUG false
#define DEBUG_SERIAL if(DEBUG)Serial

const char* clear_page = R"=====(<!DOCTYPE html>
<html>
    <meta charset="UTF-8">
    <head>
        <title>CLEAR DATA</title> 
        <link rel="preconnect" href="https://fonts.gstatic.com">
        <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@100&display=swap" rel="stylesheet"> 
        <!--
        <link rel="icon" href="https://www.flaticon.com/svg/vstatic/svg/564/564619.svg?token=exp=1612209196~hmac=9c7c1fcb73207203a4d9966a875574c8">
        -->
        <link rel="icon" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAAlwSFlzAAAAbwAAAG8B8aLcQwAAABl0RVh0U29mdHdhcmUAd3d3Lmlua3NjYXBlLm9yZ5vuPBoAAAHcSURBVDiNfZNNaxNRFIafc+dOZjJpYkKjNWCttbGRVkFIobgQKuJGREVw49q1INVdURdu7ULob9CFi25cuPIXpC3FnVKVNsFWSahhbDJh5rqRlMnXgbt53+e8nHPgYoxh6AP1CfQoRjGkqlJ6UqP022Yq2JTz68O4gQF7Urplo1fzZHMzeCJYd7dk5sUgVowxMeGrXHA81OdxskWdHOOn62M1mnzjb0eh84vmy5+RE6TQy0ncoo3mYH2N2oePRIRM4NoJovcjV6hJcRLMShoPgPBsgVA7AHhAgLm5LdOLQwME/TpNKqkGnCbCMEmSNhKboktWpXTDwnrg4fY1H1eIgz6zLcWnsYCKLNiCepMhFcOl6SMmimmnsPEJX1VkwQPQAKfxH7sk5hLYMXj80QrBVK5nlZAJXOeQxjvgjvohcwUFLzP/D3e8nCIoz9Naug5axywP6GBub8nsFesZ+dU0yasOiRjUulamvvacg9w5nKiFvbHZ9QyQISGHBEsKzD23pxnA3tlFNX2UgF3Z6PMVhjbRrDbwvU3npIcVA6y9fQrl+4h7hNTrfQERgkaOZJeLlxXRW4U1L30YVAn6tBBDh6idQj3s/oVfcmnMx9e9cAM50atloTVtdvYB/gG0QbCJx4LfUQAAAABJRU5ErkJggg==">
    </head>
    <style>
        .clear_button {
            box-shadow: 0px 0px 25px 0px #ff0000;
            background-color:#101113;
            border:2px solid #ff0000;
            display:inline-block;
            cursor:pointer;
            color:#ffffff;
            font-family:Courier New;
            font-size:40px;
            padding:40px 120px;
            text-decoration:none;
        }
        .clear_button:hover {
            background-color:#383838;
        }
        .clear_button:active {
            position:relative;
            top:1px;
        }
        div{
            position: absolute;
            top: 50%;
            left: 50%;
            margin: -90px 0 0 -290px;
        }
    </style>
    <body style="background-color: #2f3036; ">
        <div>
            <a href="/kfkdpakfpdask" class="clear_button">CLEAR ALL DATA</a>
        </div>
    </body> 
</html>)=====";




const char* ssid     = "TP-LINK7";
const char* password = "koraga23";

DHT dht(DHTPIN, DHTTYPE);

char buffer[BUFFER_SIZE];

// Set web server port number to 80
WiFiServer server(80);

WiFiUDP Udp;
IPAddress pool_ip(162,159,200,123); 
unsigned int udp_port = 123;


WiFiUDP WOL_UDP;
IPAddress computer_ip(255,255,255,255); 
byte mac[] = { 0x00, 0xd8, 0x61, 0xbf, 0x38, 0x88 };

// Variable to store the HTTP request
String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 

// timer for DHT11
unsigned long last_sample = millis();

unsigned long last_write = millis();

unsigned long current_time_unix = 1612029315;
unsigned long last_update_timestamp = millis();

float temperature_celsius = 25.5f;
float humidity = 45.5f;

float temperature_array[SAMPLE_COUNT];
float humidity_array[SAMPLE_COUNT];

struct data_struct{
  File data_file;

  bool new_data = 0;

  bool begin(){
		// if data0.bin doesnt exist, create it
		if(!LittleFS.exists("/data0.bin")){
			data_file = LittleFS.open("/data0.bin", "w");
			if(!data_file){
				DEBUG_SERIAL.println("file data0.bin creation fail");
        return false;
      }
			else{
				data_file.close();
			
				DEBUG_SERIAL.println("file data1.bin created");
			}
		}

		// if data1.bin doesnt exist, create it
		if(!LittleFS.exists("/data1.bin")){
			data_file = LittleFS.open("/data1.bin", "w");
			if(!data_file){
				DEBUG_SERIAL.println("file data1.bin creation fail");
        return false;
      }
			else{
				data_file.close();

				DEBUG_SERIAL.println("file data1.bin created");
			}
		}

    DEBUG_SERIAL.print("data from EEPROM: ");
    new_data = (bool)EEPROM.read(0);
    DEBUG_SERIAL.println(new_data);

    return true;
  }

  int readBytes(char* data_buffer, int max_length){


		// if not already reading
		if(!data_file){

      DEBUG_SERIAL.println("reading data - start");

			if(new_data == 0){

        DEBUG_SERIAL.println("new_data == 0");

				// check if old file is empty
				data_file = LittleFS.open("/data1.bin", "r");

        DEBUG_SERIAL.printf("size: %d\n", data_file.size());
				if(data_file.size() == 0){
          DEBUG_SERIAL.println("opening second file");

					data_file.close();
					// open new file if yes
					data_file = LittleFS.open("/data0.bin", "r");
				}
				// keep old file for read
			}
			else{

        DEBUG_SERIAL.println("new_data == 1");

				// check if old file is empty
				data_file = LittleFS.open("/data0.bin", "r");

        DEBUG_SERIAL.printf("size: %d\n", data_file.size());
				if(data_file.size() == 0){
          
          DEBUG_SERIAL.println("opening second file");

					data_file.close();
					// open new file if yes
					data_file = LittleFS.open("/data1.bin", "r");
				}
				// keep old file for read
			}
		}else
    {
      DEBUG_SERIAL.println("reading data - continuing");
    }
    

		// calculate how much file left to read in current file
		int remaining_bytes = data_file.size() - data_file.position();
		
		// check if this is newest file
		String current_name = data_file.name();
		bool current_file = current_name[4] == '0' ? 0 : 1;
		bool stop_after_current_file = (current_file == new_data);

    
    DEBUG_SERIAL.print("current file: ");
    DEBUG_SERIAL.println(current_name);
    String debug_str = "fileN detected: ";
    debug_str += (char)(current_file + '0');
    DEBUG_SERIAL.println(debug_str);


    // store read bytes
		int read_bytes = 0;
		
    // if we can fit remaining data to buffer
		if(remaining_bytes <= max_length){
      
      
      DEBUG_SERIAL.println("we can fit remaining data to buffer");

			data_file.readBytes(data_buffer, remaining_bytes);
			read_bytes += remaining_bytes;

      // if we still have data to read
			if(!stop_after_current_file){
				data_file.close();


        DEBUG_SERIAL.println("moving to second file");

        // open second data file
				if(new_data == 0)
					data_file = LittleFS.open("/data0.bin", "r");
				else 
					data_file = LittleFS.open("/data1.bin", "r");

        // calculate how much more data do we have to send
				remaining_bytes = data_file.size() - data_file.position();

        // calculate remaining buffer space
        int remaining_buffer_space = max_length - read_bytes;
        
        // fill buffer
        if(remaining_bytes <= remaining_buffer_space){
          DEBUG_SERIAL.println("can fit second file to buffer");
          data_file.readBytes(data_buffer + read_bytes, remaining_bytes);
          read_bytes += remaining_bytes;
			    data_file.close();
        }else{
          DEBUG_SERIAL.println("cannot fit second file to buffer");
          data_file.readBytes(data_buffer + read_bytes, remaining_buffer_space);
          read_bytes = -1;
        }
			}else{
        DEBUG_SERIAL.println("stopping after this file");
			  data_file.close();
      }

		}else{ // if we cannot fit all data in the buffer send -1
      DEBUG_SERIAL.println("cannot fit all data in the buffer");
			data_file.readBytes(data_buffer, max_length);
			read_bytes = -1;
		}

    DEBUG_SERIAL.printf("read_bytes: %d\n", read_bytes);
		
		return read_bytes;
  }

	int getSize(){
    int size = 0;

    data_file = LittleFS.open("/data0.bin", "r");
    size += data_file.size();
    data_file = LittleFS.open("/data1.bin", "r");
    size += data_file.size();

    data_file.close();

		return size;
	}

	bool writeEntry(unsigned long _time, float _temp, float _humidity){
		if(openFileForAppend()){
			
			DEBUG_SERIAL.println("file opened");

      unsigned char* tb = (unsigned char *)&_time;

      // switch endianness
      unsigned char time_buffer_inv[4] = {tb[3], tb[2], tb[1], tb[0]};

      // write to file
      data_file.write(time_buffer_inv, 4);

      // offset temperature value to fit in 8 bits with sign and 2 bits for fraction
      _temp -= 16.0f;

      // save sign bit
      bool negative = false;
      if(_temp < 0){
        _temp *= -1;
        negative = true;
      }

      // get fraction
      float fraction = _temp - (int)_temp;
      int fraction_c = fraction / 0.25f;
      int temp_c = (int)_temp;
      char res = *(const char*)&temp_c << 2;

      DEBUG_SERIAL.print("first res: ");
      DEBUG_SERIAL.println(res, BIN);

      // put fraction in last two bits
      res |= (fraction_c & 0b00000011);

      DEBUG_SERIAL.print("second res: ");
      DEBUG_SERIAL.println(res, BIN);

      res &= 0b01111111; // clear sign bit
      if(negative)
        res ^= 0b10000000;

      DEBUG_SERIAL.printf("_temp: %f\nnegative: %d\nfraction: %d\ntemp_c: %d\nres: ", _temp, (int)negative, fraction_c, temp_c);
      DEBUG_SERIAL.println(res, BIN);

      // write to file
			data_file.write(res);

      fraction = _humidity - (int)_humidity;
      fraction_c = fraction / 0.5f;
      int hum_c = (int)_humidity;

      DEBUG_SERIAL.printf("hum_c: %d fraction_c: %d",hum_c, fraction_c);

      res = (*(const char*)&hum_c) << 1;

      DEBUG_SERIAL.print("first res: ");
      DEBUG_SERIAL.println(res, BIN);

      res |= (fraction_c & 0b00000001);

      DEBUG_SERIAL.printf("_hum: %f\nnegative: %d\nfraction: %d\nhum_c: %d\nres: ", _humidity, (int)negative, fraction_c, hum_c);
      DEBUG_SERIAL.println(res, BIN);

			data_file.write(res);
			data_file.close();
			return true;
		}else{
			DEBUG_SERIAL.println("filed not opened");
			return false;
		}
	}

	void eraseAllData(){
		// truncate files	
		data_file = LittleFS.open("/data0.bin", "w");
		data_file.close();
		data_file = LittleFS.open("/data1.bin", "w");
		data_file.close();

		new_data = 0;
    EEPROM.write(0, false);
    EEPROM.commit();
	}

	bool openFileForAppend(){
		if(new_data == 0){


			DEBUG_SERIAL.println("opening data0.bin");
			// open most recent file
			data_file = LittleFS.open("/data0.bin", "a");
			
			if(data_file){
				
				DEBUG_SERIAL.println("open successful");

				// if file is max size, switch files
				if(data_file.size() == MAX_FILE_SIZE){

					DEBUG_SERIAL.println("swapping files");

          data_file.close();
					// open second file for append
					data_file = LittleFS.open("/data1.bin", "w");
					
					// switch file order
					new_data = 1;
          EEPROM.write(0, 1);
          EEPROM.commit();
					DEBUG_SERIAL.println("new data update");
				}
			}
			DEBUG_SERIAL.print("returning data_file: ");
			
			DEBUG_SERIAL.println((bool)(data_file == true));
			return data_file;
		}else{

			DEBUG_SERIAL.println("opening data1.bin");

			// open most recent file
			data_file = LittleFS.open("/data1.bin", "a");
			
			if(data_file){

				DEBUG_SERIAL.println("open successful");

				// if file is max size, switch files
				if(data_file.size() == MAX_FILE_SIZE){
          
					DEBUG_SERIAL.println("swapping files");
					
          data_file.close();
					// open second file for append
					data_file = LittleFS.open("/data0.bin", "w");
					
					// switch file order
					new_data = 0;
          EEPROM.write(0, 0);
          EEPROM.commit();
					DEBUG_SERIAL.println("new data update");
				}
			}
			return data_file;
		}
	}
};

data_struct data;

int entry_pointer = 0;
int entry_counter = 0;

bool halt = false;

WiFiClient server_client;

void sendWOL();
void handleClient(WiFiClient &client);
bool sampleDHT();
void updateTimeApprox();
void initTime();
void checkWolUDP();
unsigned long getTime();
bool enterData();

void checkHold(bool input){
  if(!input)
    halt = true;
}

void setup() {
  // init built-in led
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  // init DEBUG_SERIAL
  DEBUG_SERIAL.begin(115200);
  
  // small delay
  delay(5000);

  // init Wi-Fi
  WiFi.mode(WIFI_STA);
  DEBUG_SERIAL.print("Connecting to ");
  DEBUG_SERIAL.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    DEBUG_SERIAL.print(".");
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  
  digitalWrite(LED_BUILTIN, LOW);

  // Print local IP address and start web server
  DEBUG_SERIAL.println("");
  DEBUG_SERIAL.println("WiFi connected.");
  DEBUG_SERIAL.println("IP address: ");
  DEBUG_SERIAL.println(WiFi.localIP());
  
  // start server
  server.begin();

  // init dht
  dht.begin();

  // init littleFS
  checkHold(LittleFS.begin());

  // init EEPROM
  EEPROM.begin(512);

	// init data struct
	checkHold(data.begin());

  // init WoL boot
  WOL_UDP.begin(WOL_LISTEN_PORT);

  // update time
  initTime();
}

void loop(){
  // if issues on setup, sleep with builtin_led on
  if(halt){
    digitalWrite(LED_BUILTIN, LOW);
    DEBUG_SERIAL.println("halt");
    delay(1000);
    return;
  }

  // disable LED
  digitalWrite(LED_BUILTIN, HIGH);

  // check   for incoming clients
  server_client = server.available();   

  // handle incoming requests
  if (server_client){
    digitalWrite(LED_BUILTIN, LOW);
    handleClient(server_client);
  }
  digitalWrite(LED_BUILTIN, HIGH);


  // check WoL
  checkWolUDP();

  // sample data
  if(millis() - last_sample >= DHT_SAMPLE_INT){
    if(sampleDHT())
      DEBUG_SERIAL.println("sample success");
    else
      DEBUG_SERIAL.println("sample fail");
  }

  // write data
  if(millis() - last_write >= DHT_WRITE_INT){
    if(enterData())
      DEBUG_SERIAL.println("write success");
    else
      DEBUG_SERIAL.println("write fail");
  }
}

void handleClient(WiFiClient &client){
  DEBUG_SERIAL.println("New Client.");          // print a message out in the DEBUG_SERIAL port
  String currentLine = "";                // make a String to hold incoming data from the client
  currentTime = millis();
  previousTime = currentTime;
  char request_type = '0';
  while (client.connected() && currentTime - previousTime <= HTTP_TIMEOUT) { // loop while the client's connected
    currentTime = millis();         

    if (client.available()) {             // if there's bytes to read from the client,

      char c = client.read();             // read a byte, then
      header += c;
      if (c == '\n') {                    // if the byte is a newline character
      
        if (currentLine.length() == 0) {
          
          client.println("HTTP/1.1 200 OK");

          if(request_type == 'R'){

            // Display the HTML web page
            String name = PAGE_FILE_NAME;
            String str ="/"+name;
            File page_file = LittleFS.open(str, "r");
            if(!page_file){
              DEBUG_SERIAL.println("page_file open failed");
            }else{
              client.println("Content-type:text/html");
              client.println("Connection: keep-alive");
              client.printf("Content-length: %d\n", page_file.size());
              client.println();
              
              int size = page_file.size();
              int remaining_bytes = size;
              while(remaining_bytes > 0){
                if(remaining_bytes >= BUFFER_SIZE){
                  page_file.readBytes(buffer, BUFFER_SIZE);
                  
                  client.write(buffer, BUFFER_SIZE);

                  remaining_bytes -= BUFFER_SIZE;
                }else{
                  page_file.readBytes(buffer, remaining_bytes);
                  client.write(buffer, remaining_bytes);
                  remaining_bytes = 0;
                }
              } 
            }
            
          }
          else if(request_type == 'T'){
            client.println("Content-type:application/octet-stream");
            client.println("Accept-Ranges:bytes");
            client.printf("Content-length: %d\n", data.getSize());
            client.println("Connection: keep-alive");
            client.println("Access-Control-Allow-Origin: *");
            client.println();

            int bytes_read = -1;
            while(bytes_read == -1){

              bytes_read = data.readBytes(buffer, BUFFER_SIZE);
                
              int write_to_buffer = bytes_read == -1 ? BUFFER_SIZE : bytes_read;
              client.write(buffer, write_to_buffer);
            } 
          }     
          else if(request_type == 'C'){
            client.println("Content-type:text/html");
            client.println("Connection: keep-alive");
            client.println();
            client.print(clear_page);
          }

          // The HTTP response ends with another blank line
          client.println();
          client.println();

          // Break out of the while loop
          break;
        } 
        else { // if you got a newline, then clear currentLine
          DEBUG_SERIAL.println(currentLine);
          if(currentLine.indexOf("GET / HTTP/1.1") != -1){
            DEBUG_SERIAL.println("RECEIVED GET ROOT REQUEST");
            request_type = 'R';
          }
          else if(currentLine.indexOf("GET /temperature.bin HTTP/1.1") != -1){
            DEBUG_SERIAL.println("RECEIVED GET TEMPERATURE.BIN REQUEST");
            request_type = 'T';
          }

          else if(currentLine.indexOf("GET /clear_data HTTP/1.1") != -1){
            DEBUG_SERIAL.println("RECEIVED CLEAR REQUEST");
            request_type = 'C';
          }

          else if(currentLine.indexOf("GET /kfkdpakfpdask HTTP/1.1") != -1){
            DEBUG_SERIAL.println("RECEIVED CONFIRMED CLEAR REQUEST");
            data.eraseAllData();
            request_type = 'R';
          }

          currentLine = "";
        }
      } 
      else if (c != '\r') {  // if you got anything else but a carriage return character,
        currentLine += c;      // add it to the end of the currentLine
      }
    }
  }
  



  // clear the header variable
  header = "";
  // close the connection
  client.stop();
  DEBUG_SERIAL.println("Client disconnected.");
}

#if false
bool sampleDHT(){
		DEBUG_SERIAL.println("sampling DHT");

    digitalWrite(LED_BUILTIN, LOW);

    // float temp_samples[3];
    // float hum_samples[3];
    temperature_celsius = dht.readTemperature();
    humidity = dht.readHumidity();
    temperature_celsius = 0.0f;
    humidity = 0.0f;
    for(int i = 0; i < 2; i++){
      delay(500);
      temperature_celsius += dht.readTemperature();
      humidity += dht.readHumidity();
    }

    temperature_celsius /= 2.0f;
    humidity /= 2.0f;


    digitalWrite(LED_BUILTIN, HIGH);

    // get time from NTP server
		current_time_unix = getTime();

		DEBUG_SERIAL.println("writing entry");
		DEBUG_SERIAL.printf("temp: %f'\n", temperature_celsius);
		DEBUG_SERIAL.printf("hum: %f'\n", humidity);

    bool res = false;
    // check if data is good (DHT11 is not that reliable)
    bool good_readings = (temperature_celsius > -50) && (temperature_celsius < 50) && (humidity >= 0) && (humidity <= 100);
    
		DEBUG_SERIAL.printf("good readings: %d\n", (int)good_readings);
    if(good_readings){
      res = data.writeEntry(current_time_unix, temperature_celsius, humidity);
      if(res){
        last_sample = millis();
      }
    }

    return res;
}
#else
bool sampleDHT(){
    unsigned int _temp_timestamp = millis();

		DEBUG_SERIAL.println("sampling DHT");

    //digitalWrite(LED_BUILTIN, LOW);

    float temp_temperature = dht.readTemperature();
    float temp_humidity = dht.readHumidity();

    //digitalWrite(LED_BUILTIN, HIGH);

		DEBUG_SERIAL.println("sampled DHT");
		DEBUG_SERIAL.printf("temp: %f'\n", temp_temperature);
		DEBUG_SERIAL.printf("hum: %f'\n", temp_humidity);

    bool res = false;
    // check if data is good (DHT11 is not that reliable)
    bool good_readings = (temp_temperature > -50) && (temp_temperature < 50) && (temp_humidity >= 0) && (temp_humidity <= 100);
    
		DEBUG_SERIAL.printf("good readings: %d\n", (int)good_readings);
    if(good_readings){
      
      if(entry_pointer < SAMPLE_COUNT){

		    DEBUG_SERIAL.printf("entry pointer: %d\nentry counter: %d\n", entry_pointer, entry_counter);

        temperature_array[entry_pointer] = temp_temperature;
        humidity_array[entry_pointer] = temp_humidity;

        entry_pointer++;
        entry_counter++;

        entry_pointer %= SAMPLE_COUNT;
      }

      last_sample = _temp_timestamp;
    }


    return res;
}
#endif
bool enterData(){
    unsigned int _temp_timestamp = millis();

		DEBUG_SERIAL.println("writing data entry DHT");

    digitalWrite(LED_BUILTIN, LOW);

    // get entry counter
    int count = std::min(SAMPLE_COUNT, entry_counter);


		DEBUG_SERIAL.print("count: ");
		DEBUG_SERIAL.println(count);

    // sort arrays
    std::sort(std::begin(temperature_array), (std::begin(temperature_array)+ count));
    std::sort(std::begin(humidity_array), (std::begin(humidity_array) + count));

    // get avg of 2 middle elements if even number of elements
    if(count > 1 && count % 2 == 0){
		  DEBUG_SERIAL.println("getting avg from even");
      temperature_celsius = (temperature_array[(int)(count/2) - 1] + temperature_array[(int)(count/2)]) / 2.0f;
      humidity = (humidity_array[(int)(count/2) - 1] + humidity_array[(int)(count/2)]) / 2.0f;
    }else{ // if not, get middle element
		  DEBUG_SERIAL.println("getting middle element");
      temperature_celsius = temperature_array[(int)(count/2)];
      humidity = humidity_array[(int)(count/2)];
    }

    digitalWrite(LED_BUILTIN, HIGH);

    // get time from NTP server
		unsigned int time_unix = getTime();
    current_time_unix = time_unix;

    // move timestep to center of sampling window
    time_unix -= ((DHT_WRITE_INT/2) / 1000);

		DEBUG_SERIAL.println("writing entry");
		DEBUG_SERIAL.printf("temperature: %f'\n", temperature_celsius);
		DEBUG_SERIAL.printf("humidity: %f'\n", humidity);

    bool res = false;
    // check if data is good (DHT11 is not that reliable)
    //bool good_readings = (temperature_celsius > -50) && (temperature_celsius < 50) && (humidity >= 0) && (humidity <= 100);
    
		//DEBUG_SERIAL.printf("good readings: %d\n", (int)good_readings);
    //if(good_readings){
    int counter = 0;
    while(!res && counter < 10){
      res = data.writeEntry(time_unix, temperature_celsius, humidity);
      if(res){
        last_write = _temp_timestamp;
        entry_counter = 0;
        entry_pointer = 0;
      }
    }
    //}

    return res;
}

unsigned long getTime(){

  char packet[48];

  packet[0] = 0xE3;
  packet[2] = 0x06;
  packet[3] = 0xEC;
  packet[12]= 0x49;
  packet[13]= 0x4E;
  packet[14]= 0x49;
  packet[15]= 0x52;

  Udp.begin(udp_port);


  int packetSize = 0;
  unsigned long c_time;
  for(int i = 0; i < UDP_RETRY; i++){
    if(packetSize > 0)
      break;

    DEBUG_SERIAL.printf("Sending UDP: %d\n", i);

    Udp.beginPacket(pool_ip, udp_port);
    Udp.write(packet, 48);
    Udp.endPacket();

    unsigned long start_timestamp = millis();
    while(millis() - start_timestamp <= UDP_TIMEOUT){
      packetSize = Udp.parsePacket();
      if (packetSize)
      {
        DEBUG_SERIAL.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
        int len = Udp.read(packet, 48);
        if (len > 0)
        {
          packet[len] = '\0';
        }
        DEBUG_SERIAL.println("UDP packet contents:");
        for(int i = 0; i < 48; i++){
          DEBUG_SERIAL.print(packet[i], HEX);
          DEBUG_SERIAL.print(' ');
        }
        
        
        //unsigned long *c_time = (unsigned long *)&packet[40];
        *(((char*)&c_time) + 3) = packet[40];
        *(((char*)&c_time) + 2) = packet[41];
        *(((char*)&c_time) + 1) = packet[42];
        *(((char*)&c_time) + 0) = packet[43];

        c_time -= 2208988800UL;

        DEBUG_SERIAL.println();
        DEBUG_SERIAL.print("Current time: ");
        DEBUG_SERIAL.println(c_time);
        break;
      }
    }
  }
  if(packetSize > 0 && abs(c_time - current_time_unix) < 10 * DHT_SAMPLE_INT){
    return c_time;
  }else{
    updateTimeApprox();
    return current_time_unix;
  }
}

void updateTimeApprox(){
  unsigned long tt = millis();
  current_time_unix += ((tt - last_update_timestamp)/1000);
  last_update_timestamp = tt;
}

void initTime(){

  char packet[48];

  packet[0] = 0xE3;
  packet[2] = 0x06;
  packet[3] = 0xEC;
  packet[12]= 0x49;
  packet[13]= 0x4E;
  packet[14]= 0x49;
  packet[15]= 0x52;

  Udp.begin(udp_port);


  int packetSize = 0;
  unsigned long c_time;
  int i = 0;
  while(packetSize == 0){

    DEBUG_SERIAL.printf("Sending UDP: %d\n", i);
    i++;

    Udp.beginPacket(pool_ip, udp_port);
    Udp.write(packet, 48);
    Udp.endPacket();

    unsigned long start_timestamp = millis();
    while(millis() - start_timestamp <= UDP_TIMEOUT){
      packetSize = Udp.parsePacket();
      if (packetSize)
      {
        DEBUG_SERIAL.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
        int len = Udp.read(packet, 48);
        if (len > 0)
        {
          packet[len] = '\0';
        }
        DEBUG_SERIAL.println("UDP packet contents:");
        for(int i = 0; i < 48; i++){
          DEBUG_SERIAL.print(packet[i], HEX);
          DEBUG_SERIAL.print(' ');
        }
        
        
        //unsigned long *c_time = (unsigned long *)&packet[40];
        *(((char*)&c_time) + 3) = packet[40];
        *(((char*)&c_time) + 2) = packet[41];
        *(((char*)&c_time) + 1) = packet[42];
        *(((char*)&c_time) + 0) = packet[43];

        c_time -= 2208988800UL;

        DEBUG_SERIAL.println();
        DEBUG_SERIAL.print("Current time: ");
        DEBUG_SERIAL.println(c_time);
        break;
      }
    }
  }

  last_update_timestamp = millis();
  current_time_unix = c_time;
}

void checkWolUDP(){
  int packetSize = WOL_UDP.parsePacket();
  if (packetSize) {
    digitalWrite(LED_BUILTIN, LOW);
    DEBUG_SERIAL.println("Sending WOL Packet...");
    WOL_UDP.stop();
    WOL_UDP.begin(9);
    WakeOnLan::sendWOL(computer_ip, WOL_UDP, mac, sizeof mac);
    WOL_UDP.stop();
    WOL_UDP.begin(WOL_LISTEN_PORT);
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

