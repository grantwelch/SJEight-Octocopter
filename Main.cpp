//Globals for now
uint16_t duty_cycle = 0;
uint8_t dc_low_byte = 0;
char dc_high_byte = 0;
uint8_t slave = 0x40<<1; // 8 bit addr
uint8_t mode1_reg = 0x0;
uint8_t prescale_reg = 0xFE;
uint8_t prescale_value = 0x80;
uint8_t sleep = 0x10;
uint8_t initialize = 0xA1;

typedef enum { //enumeration is a numbered list of variables
   IMU_Q,
   controller_Q
} sharedHandleId_t;
#define quesize 100

class readController : public scheduler_task { //activate on the XBEE SJone only
   public:
     readController(uint8_t priority) : scheduler_task("readController", 2048, priority) {
     QueueHandle_t q = xQueueCreate(1, quesize); //create queue
     addSharedObject(controller_Q, q); //add my sensor to the queue
   };
   bool run(void *p) {
     TickType_t xLastWakeTime;
     const TickType_t xFrequency = 210;
     xLastWakeTime = xTaskGetTickCount();
     char value[quesize] = {0};
     Uart3& u3 = Uart3::getInstance();
     u3.init(9600); //baud rate
     u3.gets(&value[0],quesize,100);
     puts(value);
     xQueueSend(getSharedObject(controller_Q), &value, portMAX_DELAY); //sends the data to the queue to be processed
     vTaskDelayUntil( &xLastWakeTime, xFrequency/portTICK_PERIOD_MS );
     return true;
   }
};

class processController : public scheduler_task //activate on the XBEE controller only
{
  public:
      processController(uint8_t priority) : scheduler_task("processController", 2048, priority) { }
      bool run(void *p) {
          char valuebuffer[quesize] = {0};
          QueueHandle_t q_name = getSharedObject(controller_Q); //create the q
          if (xQueueReceive(q_name, &valuebuffer, portMAX_DELAY)) {
             printf("\n");
             printf(valuebuffer);
             printf("\n");

             }
          return true;
      }
};

class calculateIMU : public scheduler_task //create a task on the AHREF board only
{
  public:
      calculateIMU(uint8_t priority) : scheduler_task("calculate_heading", 2048, priority) { }
      bool run(void *p) {
          char valuebuffer[quesize] = {0};
          char * tokens[3];
          uint8_t synch_counter = 0;
          uint8_t buffer_index = 0;
          float yaw=0;
          float pitch=0;
          float roll=0;
          size_t maxTokens = 0;
          const char hashtag = '#';
          QueueHandle_t q_name = getSharedObject(IMU_Q); //create the q
          if (xQueueReceive(q_name, &valuebuffer, portMAX_DELAY)) {
             printf("\n");
             
             char *tokens[4];
             char *p;
             p = strtok(valuebuffer,",");
             int j = 0;

             while (p) {
                 tokens[j++] = p;
                 p = strtok(NULL, ",");
             }
             yaw = str::toFloat(tokens[1]);
             pitch = str::toFloat(tokens[2]);
             roll = str::toFloat(tokens[3]);

             printf("\n");
             printf("\nYAW:%.3f",yaw);
             printf("     ");
             printf("PITCH:%.3f",pitch);
             printf("     ");
             printf("ROLL:%.3f",roll);
          }
          return true;
      }
   
};
 
class readIMU : public scheduler_task { //create task on AHREF board only
    public:
        readIMU(uint8_t priority) : scheduler_task("read_AHREF", 2048, priority) {
        QueueHandle_t q = xQueueCreate(1, quesize); //create queue
        addSharedObject(IMU_Q, q); //add my sensor to the queue
    };
    bool run(void *p) {
        TickType_t xLastWakeTime;
        const TickType_t xFrequency = 20;
        xLastWakeTime = xTaskGetTickCount();
        char value[quesize] = {0};
        Uart3& u3 = Uart3::getInstance();
        u3.init(115200); //baud rate
        u3.putline("#s<12>");
        u3.gets(&value[0],quesize,100);
        xQueueSend(getSharedObject(IMU_Q), &value, portMAX_DELAY); //sends the data to the queue to be processed
        vTaskDelayUntil( &xLastWakeTime, xFrequency/portTICK_PERIOD_MS );
        return true;
    }
}

void initializeMotors (void) {
    I2C2& i2c = I2C2::getInstance(); // Get I2C driver instance
    i2c.init(100);
    //initialize
    i2c.writeReg(slave, mode1_reg, sleep); //sleep
    delay_ms(1);
    i2c.writeReg(slave, prescale_reg, prescale_value); //set clock
    delay_ms(5);
    i2c.writeReg(slave, mode1_reg, initialize); //wake up, get going
    delay_ms(1);
    //send the maximum signal
    for(int i = 0; i<3;i++) {
        if(i == 0) {
            duty_cycle = (4*4096)/100;
            dc_low_byte = duty_cycle;
            dc_high_byte = duty_cycle >> 8;
        }
        if(i == 1) {
            duty_cycle = (10.5*4096)/100;
            dc_low_byte = duty_cycle;
            dc_high_byte = duty_cycle >> 8;
        }
        if(i == 2) {
            duty_cycle = (4*4096)/100;
            dc_low_byte = duty_cycle;
            dc_high_byte = duty_cycle >> 8;
        }
        for (int n = 6; n < 66;) {
            i2c.writeReg(slave, n, 0x0); //low byte
            delay_ms(1);
            i2c.writeReg(slave, n+1, 0x0); //high byte
            delay_ms(1);
            i2c.writeReg(slave, n+2, dc_low_byte); //low byte
            delay_ms(1);
            i2c.writeReg(slave, n+3, dc_high_byte); //high byte
            delay_ms(1);
            n=n+4;
        }
            //send the off signal
        for (int n = 6; n < 66; n++) {
            i2c.writeReg(slave, n, 0); //off
            delay_ms(1);
        }
    }
}

void motorPWM (void) {
     printf("\n Starting while \n");
     scanf("%f", &user_input);
     printf("\n %f", user_input);
     duty_cycle = (user_input*4096)/100;
     printf("\n %f",user_input);
     printf("\n Duty Cycle: ");
     printf("%d",duty_cycle);
     if(duty_cycle) {
         dc_low_byte = duty_cycle;
         dc_high_byte = duty_cycle >> 8;
         for (int n = 6; n < 66;) {
             i2c.writeReg(slave, n, 0x0); //low byte
             delay_ms(1);
             i2c.writeReg(slave, n+1, 0x0); //high byte
             delay_ms(1);
             i2c.writeReg(slave, n+2, dc_low_byte); //low byte
             delay_ms(1);
             i2c.writeReg(slave, n+3, dc_high_byte); //high byte
             delay_ms(1);
             n=n+4;
         }
         duty_cycle = 0;
     }    
}
int main(void) {

    //Add tasks depending on which board you are loading the code too


    /**
     * A few basic tasks for this bare-bone system :
     *      1.  Terminal task provides gateway to interact with the board through UART terminal.
     *      2.  Remote task allows you to use remote control to interact with the board.
     *      3.  Wireless task responsible to receive, retry, and handle mesh network.
     *
     * Disable remote task if you are not using it.  Also, it needs SYS_CFG_ENABLE_TLM
     * such that it can save remote control codes to non-volatile memory.  IR remote
     * control codes can be learned by typing the "learn" terminal command.
     */
    scheduler_add_task(new terminalTask(PRIORITY_HIGH));

    /* Consumes very little CPU, but need highest priority to handle mesh network ACKs */
    scheduler_add_task(new wirelessTask(PRIORITY_CRITICAL));

    /* The task for the IR receiver */
    // scheduler_add_task(new remoteTask  (PRIORITY_LOW));

    /* Your tasks should probably used PRIORITY_MEDIUM or PRIORITY_LOW because you want the terminal
     * task to always be responsive so you can poke around in case something goes wrong.
     */

    /**
     * This is a the board demonstration task that can be used to test the board.
     * This also shows you how to send a wireless packets to other boards.
     */
    #if 0
        scheduler_add_task(new example_io_demo());
    #endif

    /**
     * Change "#if 0" to "#if 1" to enable examples.
     * Try these examples one at a time.
     */
    #if 0
        scheduler_add_task(new example_task());
        scheduler_add_task(new example_alarm());
        scheduler_add_task(new example_logger_qset());
        scheduler_add_task(new example_nv_vars());
    #endif

    /**
     * Try the rx / tx tasks together to see how they queue data to each other.
     */
    #if 0
        scheduler_add_task(new queue_tx());
        scheduler_add_task(new queue_rx());
    #endif

    /**
     * Another example of shared handles and producer/consumer using a queue.
     * In this example, producer will produce as fast as the consumer can consume.
     */
    #if 0
        scheduler_add_task(new producer());
        scheduler_add_task(new consumer());
    #endif

    /**
     * If you have RN-XV on your board, you can connect to Wifi using this task.
     * This does two things for us:
     *   1.  The task allows us to perform HTTP web requests (@see wifiTask)
     *   2.  Terminal task can accept commands from TCP/IP through Wifly module.
     *
     * To add terminal command channel, add this at terminal.cpp :: taskEntry() function:
     * @code
     *     // Assuming Wifly is on Uart3
     *     addCommandChannel(Uart3::getInstance(), false);
     * @endcode
     */
    #if 0
        Uart3 &u3 = Uart3::getInstance();
        u3.init(WIFI_BAUD_RATE, WIFI_RXQ_SIZE, WIFI_TXQ_SIZE);
        scheduler_add_task(new wifiTask(Uart3::getInstance(), PRIORITY_LOW));
    #endif

    scheduler_start(); ///< This shouldn't return
    return -1;
}
