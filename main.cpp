#include "mbed.h"
#include <stdbool.h>
#include <string>
//#include "WakeUp.h"
#include "SDFileSystem.h"
#define DEBUG

SDFileSystem sd(PA_7, PA_6, PA_5, PB_6, "sd"); // the pinout on the mbed Cool Components workshop board

Serial  pc(USBTX,USBRX);
Serial  wifi(PA_11, PA_12);

#define SSID "username" // insert your SSID
#define PASS "password" // insert your password
#define LOCATIONID "95131,USA" // location id
#define DST_IP "162.243.44.32" //api.openweathermap.org
//http://api.openweathermap.org/data/2.5/forecast?q=San%20Jose,USA&cnt=1

string b;
volatile bool ready=0, record = 0;
volatile int count = 0;
unsigned char data[120*160];

volatile bool json_start = 0, json_pause = 0, json_close = 0;


#ifdef __cplusplus
extern "C" {
#endif

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

int weather_data = 0;
int sprinkler_on = 0;

/* memory debugging routines */
typedef struct {
    unsigned int numFrees;
    unsigned int numMallocs;    
    /* XXX: we really need a hash table here with per-allocation
     *      information */ 
} yajlTestMemoryContext;

/* cast void * into context */
#define TEST_CTX(vptr) ((yajlTestMemoryContext *) (vptr))

static void yajlTestFree(void * ctx, void * ptr) {
    assert(ptr != NULL);
    TEST_CTX(ctx)->numFrees++;
    free(ptr);
}

static void * yajlTestMalloc(void * ctx, unsigned int sz) {
    assert(sz != 0);
    TEST_CTX(ctx)->numMallocs++;
    return malloc(sz);
}

static void * yajlTestRealloc(void * ctx, void * ptr, unsigned int sz) {
    if (ptr == NULL) {
        assert(sz != 0);
        TEST_CTX(ctx)->numMallocs++;        
    } else if (sz == 0) {
        TEST_CTX(ctx)->numFrees++;                
    }

    return realloc(ctx, sz);
}


/* begin parsing callback routines */
#define BUF_SIZE 2048

static int test_yajl_null(void *ctx) {
    printf("null\n");
    return 1;
}

static int test_yajl_boolean(void * ctx, int boolVal) {
    //printf("bool: %s\n", boolVal ? "true" : "false");
    return 1;
}

static int test_yajl_integer(void *ctx, long integerVal) {
    //printf("integer: %ld\n", integerVal);
    if(weather_data == 1){
        printf("integer: %ld\n", integerVal);
        if(integerVal<801){
			//weather info fetched from openweathermap to for algorithm to determine the best condition to turn on the sprinklers
            sprinkler_on = 1;
            printf("** AI **\n");
        }
        weather_data = 0;
    } 
    return 1;
}

static int test_yajl_double(void *ctx, double doubleVal) {
    //printf("double: %g\n", doubleVal);
    return 1;
}

static int test_yajl_string(void *ctx, const unsigned char * stringVal, unsigned int stringLen) {
    //printf("string: '");
    //fwrite(stringVal, 1, stringLen, stdout);
    //printf("'\n");   
    return 1;
}

static int test_yajl_map_key(void *ctx, const unsigned char * stringVal, unsigned int stringLen) {
    char * str = (char *) malloc(stringLen + 1);
    str[stringLen] = 0;
    memcpy(str, stringVal, stringLen);
    //printf("key: '%s'\n", str);
    if(strcmp("id",str) == 0){
        printf("key: '%s'\n", str);
        weather_data = 1;
    }
    else
        weather_data = 0;    
    free(str);
    return 1;
}

static int test_yajl_start_map(void *ctx) {
    //printf("map open '{'\n");
    return 1;
}


static int test_yajl_end_map(void *ctx) {
    //printf("map close '}'\n");
    return 1;
}

static int test_yajl_start_array(void *ctx) {
    //printf("array open '['\n");
    return 1;
}

static int test_yajl_end_array(void *ctx) {
    //printf("array close ']'\n");
    return 1;
}

static yajl_callbacks callbacks = {
    test_yajl_null,
    test_yajl_boolean,
    test_yajl_integer,
    test_yajl_double,
    NULL,
    test_yajl_string,
    test_yajl_start_map,
    test_yajl_map_key,
    test_yajl_end_map,
    test_yajl_start_array,
    test_yajl_end_array
};

/*
static void usage(const char * progname) {
    fprintf(stderr,
            "usage:  %s [options] <filename>\n"
            "   -c  allow comments\n"
            "   -b  set the read buffer size\n",
            progname);
    exit(1);
}
*/

#ifdef __cplusplus
}
#endif


void newData() {
    //buffer variables
    char lezen;
    
    //if data is ready in the buffer
    while (wifi.readable()) {
        //read 1 character
        lezen = wifi.getc();
        #ifdef DEBUG
            pc.putc(lezen);
        #endif
        //write buffer character to big buffer string
        /*Start when "{", then stop when "\r", then resume after first ":"*/
        if(record == 1){    //record for json data
            if(json_start == 0 && lezen == '{'){    //start of json data
                json_start = 1;
            }
            if(json_start == 1){
                if(json_pause == 0){
                    if(lezen == '\r')   //stop for 1024byte data break
                        json_pause = 1;
                    else{
                        data[count]=lezen;
                        count++;
                        if(json_close == 1){
                            if(lezen == '}'){//end of json data
                                json_pause = 1;
                                data[count]='\n';
                                count++;
                            }
                            else
                                json_close = 0;//cases where '}' is not followed by ']' aka end of json data
                        }
                    }    
                    if(lezen == ']')
                        json_close = 1;
                }
                else{
                    if(lezen == ':')
                        json_pause = 0; //resume recording
                }
            }
        }
        else{ //non-json record
            b += lezen;
            //if the character is # than the end of the sentence is reached and some stuff has to be done
            if (lezen == '\n') {
                b.erase(b.length()-1,1);
                if (b=="OK\r")
                    ready = 1;
                b.clear();
            }
        }
 
        //if the character is # than the end of the sentence is reached and some stuff has to be done
        /*if (lezen == '\n') {
            b.erase(b.length()-1,1);
            if (b=="OK\r"){
                ready = 1;
            }//Dont record until after POST message
            if(record==1){
                //sprintf(data, "%s", b.c_str());
                for (int i=0; i<b.length(); i++) {
                    data[count]=b[i];
                    count++;
                }
            }
            b.clear();
        }*/
    }
}


void setup()
{
    int tries = 0;
    
    pc.printf("Init.");
    while(1){
        wifi.printf("AT+RST\r"); // reset to start wifi
        wifi.attach(&newData);
        wait(1);
        if(ready == 1){
            pc.printf("Module test ok.\n");
            ready = 0;
            break;
        }
        else{
            if(tries >= 5){
                pc.printf("System Failed.");
                while(1);
            }
            tries ++;
        }
    }
    wait(1);
    tries = 0;
    while(1){
        wifi.printf("AT+CIPMUX=0\r"); // set to single connection mode
        wifi.printf("AT+CWMODE=1\r");
        string cmd="AT+CWJAP=\"";
        cmd+=SSID;
        cmd+="\",\"";
        cmd+=PASS;
        cmd+="\"\r";
        pc.printf(cmd.c_str());
        wifi.printf(cmd.c_str());
        wait(2);
        if(ready == 1){
            pc.printf("WiFi Connected.\n");
            ready = 0;
            break;
        }
        else{
            if(tries >= 5){
                pc.printf("WiFi failed.");
                while(1);
            }
            tries ++;
        }
    }
    wait(5);
}

int main()
{
    pc.baud(115200);  
    wifi.baud(115200);
    
    setup();
        
    while(1){
        //Set wakeup time for 60 seconds
        //WakeUp::set_ms(60000);
        //pc.printf("Back from sleep\n");
        //wait(2);
        string cmd = "AT+CIPSTART=\"TCP\",\"";
        cmd += DST_IP;
        cmd += "\",80\r";
        wifi.printf(cmd.c_str());
        wait(2);
        //if(Serial.find("Error")) return;
        cmd = "GET /data/2.5/forecast?q=";
        cmd += LOCATIONID;
        cmd += "&cnt=1 HTTP/1.1\r\nHost: api.openweathermap.org\r\n\r\n";
        wifi.printf("AT+CIPSEND=%d\r",cmd.length());
        wait(2);
        /*if(Serial.find(">")){
        dbgSerial.print(">");
        }else{
        Serial.println("AT+CIPCLOSE");
        dbgSerial.println("connection timeout");
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(2, 2);
        tft.setTextColor(ST7735_WHITE);
        tft.println("connection timeout");
        delay(1000);
        return;
        }*/
        wifi.printf(cmd.c_str());
        record = 1;
        wait(3);
        // Write to local file
        pc.printf("Finished\n");
        FILE *fp = fopen("/sd/json_web.txt", "w");  // Open "test.txt" on the local file system for appending
        for (int i=0; i<count; i++) {
            fprintf(fp, "%c",data[i]);
        }
        fclose(fp);
        record = 0;
        count = 0;
        //#if defined(__MICROLIB) && defined(__ARMCC_VERSION) // with microlib and ARM compiler
            free(fp);
        //#endif
        pc.printf("End of SD write\n");
        
        json_start = 0;
        json_pause = 0;
        json_close = 0;
        //pc.printf("Entering Deepsleep\n");        
        //deepsleep();
        wait(2);
        pc.printf("\n\n\nReady to parse JSON\n\n\n");
        wait(2);
        yajl_handle hand;
        const char * fileName;
        static unsigned char * fileData = NULL;
        unsigned int bufSize = BUF_SIZE;
        yajl_status stat;
        size_t rd;
        yajl_parser_config cfg = { 0, 1 };
        int done;
    
        /* memory allocation debugging: allocate a structure which collects
         * statistics */
        yajlTestMemoryContext memCtx = { 0,0 };
    
        /* memory allocation debugging: allocate a structure which holds
         * allocation routines */
        yajl_alloc_funcs allocFuncs = {
            yajlTestMalloc,
            yajlTestRealloc,
            yajlTestFree,
            (void *) NULL
        };
    
        allocFuncs.ctx = (void *) &memCtx;
    
        /* check arguments.  We expect exactly one! */
        cfg.allowComments = 1;
        bufSize = 2048;
    
        fileData = new unsigned char[bufSize];
    
        if (fileData == NULL) {
            printf("failed to allocate read buffer of %u bytes, exiting.",
                    bufSize);
            exit(2);
        }
    
        fileName = "/sd/json_web.txt";
        FILE *file = fopen(fileName, "r");
        
        printf("Lets start\n");
    
        /* ok.  open file.  let's read and parse */
        hand = yajl_alloc(&callbacks, &cfg, &allocFuncs, NULL);
    
        done = 0;
        printf("While loop\n");
        while (!done) {
            rd = fread((void *) fileData, 1, bufSize, file);
            
            if (rd == 0) {
                //if (!feof(file)) {
                //    fprintf(stderr, "error reading from '%s'\n", fileName);
                //    break;
                //}
                done = 1;
            }
    
            if (done) {
                /* parse any remaining buffered data */
                stat = yajl_parse_complete(hand);
            } else {
                /* read file data, pass to parser */
                stat = yajl_parse(hand, fileData, rd);
            }
            
            if(stat != yajl_status_insufficient_data &&
               stat != yajl_status_ok) {
                unsigned char * str = yajl_get_error(hand, 0, fileData, rd);
                fflush(stdout);
                fprintf(stderr, (char *) str);
                yajl_free_error(hand, str);
                break;
            }
        } 
    
        yajl_free(hand);
        free(fileData);
    
        /* finally, print out some memory statistics */
    
    /* (lth) only print leaks here, as allocations and frees may vary depending
     *       on read buffer size, causing false failures.
     *
     *  printf("allocations:\t%u\n", memCtx.numMallocs);
     *  printf("frees:\t\t%u\n", memCtx.numFrees);
    */
        fflush(stderr);
        fflush(stdout);
        //printf("memory leaks:\t%u\n", memCtx.numMallocs - memCtx.numFrees);    
    
        pc.printf("\n\n\nDONE\n\n\n");
        wait(60000);
    }
}