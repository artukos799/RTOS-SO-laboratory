/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*
*                           (c) Copyright 1992-2002, Jean J. Labrosse, Weston, FL
*                                           All Rights Reserved
*
*                                               EXAMPLE #1
*********************************************************************************************************
*/
 
/********************************************************************************************************
*                                                                                                                                                               
*                                                                                                                               
*
*
********************************************************************************************************/
 
#include "includes.h"
 
/*
*********************************************************************************************************
*                                               CONSTANTS
*********************************************************************************************************
*/

#define  TASK_STK_SIZE                 512       /* Size of each task's stacks (# of WORDs)            */
#define  N_TASKS                       30       /* Number of identical tasks                          */  
#define  MSG_QUEUE_SIZE                11      /* Queue size                                          */
#define  TSKS_GROUP                     5      /* Number of tasks in each group (queue/mailbox/semafor */
#define  DEFAULT_WORKLOAD			    0     /* Default value of the workload, used when program just started */

INT32U WORKLOAD = DEFAULT_WORKLOAD;						/* Current workload for tasks						*/

typedef struct
{
	INT16U taskNumber;
	INT32U counter;
	INT32U workload;
	char toPrint[12];
} 
Structure;

/*
*********************************************************************************************************
*                                               VARIABLES
*********************************************************************************************************
*/
 
OS_STK        TaskStk[N_TASKS][TASK_STK_SIZE];        /* Tasks stacks                                  */
OS_STK        TaskStartStk[TASK_STK_SIZE];

OS_STK        KeyboardStk[TASK_STK_SIZE];
OS_STK        EditStk[TASK_STK_SIZE];
OS_STK        DisplayStk[TASK_STK_SIZE];
OS_STK        SendStk[TASK_STK_SIZE];

OS_STK        QTaskStk[TASK_STK_SIZE];
OS_STK        MboxTaskStk[TASK_STK_SIZE];
OS_STK        SemTaskStk[TASK_STK_SIZE];

char          TaskData[N_TASKS];                      /* Parameters to pass to each task               */
OS_EVENT	  *RandomSem;

OS_EVENT      *ToEdit;                              /* Message queue for communication between Keyboard and Edit */
void          *ToEditTb1[20];                       /* Array of pointers (20 entries)                */
OS_EVENT      *ToDisplay;                               /* Message queue for communication between Edit and Display */
void          *ToDisplayTb1[20];                       /* Array of pointers (20 entries)                */

OS_EVENT	  *WorkloadSem;						  /* Semaphore for  SemTasks						*/
OS_EVENT 	  *WorkloadQueue;					  /* Queue for QTasks								*/
void		  *WorkloadQueueTb1[20];			  /* Array of pointers (20 entries)				   */
OS_EVENT 	  *WorkloadMbox[6];					  /* Mailbox for MboxTasks						   */

OS_EVENT       *ToSend;							  /* Queue for communication between Edit and Send */
void           *ToSendTb1[MSG_QUEUE_SIZE];		  /* Array of pointers (20 entries)                */

/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/

        void  Task(void *data);                       /* Function prototypes of tasks                  */
        void  TaskStart(void *data);                  /* Function prototypes of Startup task           */
static  void  TaskStartCreateTasks(void);
static  void  TaskStartDispInit(void);
static  void  TaskStartDisp(void);
		
		void  Keyboard(void *data);					  /* Function prototypes of Keyboard task          */ 
		void  Edit(void *data);						  /* Function prototypes of Edit task              */
		void  Display(void *data);					  /* Function prototypes of Display task           */
		
		void  Send(void *data);						  /* Function prototypes of Send task           */
		void  QTask(void *data);					  /* Function prototypes of QTask				   */
		void  MboxTask(void *data);					  /* Function prototypes of MboxTask			   */
		void  SemTask(void *data);					  /* Function prototypes of SemTask				   */

/*$PAGE*/
/*
*********************************************************************************************************
*                                                MAIN
*********************************************************************************************************
*/

void  main (void)
{
	INT8U i, err;
	
    PC_DispClrScr(DISP_FGND_WHITE + DISP_BGND_BLACK);      /* Clear the screen                         */

    OSInit();                                              /* Initialize uC/OS-II                      */

    PC_DOSSaveReturn();                                    /* Save environment to return to DOS        */
    PC_VectSet(uCOS, OSCtxSw);                             /* Install uC/OS-II's context switch vector */
	
	ToEdit = OSQCreate(&ToEditTb1[0], MSG_QUEUE_SIZE); /* Creation of message queue                */
    ToDisplay = OSQCreate(&ToDisplayTb1[0], MSG_QUEUE_SIZE);/* Creation of message queue              */

	ToSend = OSQCreate(&ToSendTb1[0], MSG_QUEUE_SIZE);	   /* Creation of ToSend queue				   */
	WorkloadSem = OSSemCreate(1);						   /* Creation of Workload semaphore					   */
	WorkloadQueue = OSQCreate(&WorkloadQueueTb1[0], MSG_QUEUE_SIZE); /*Creation of Workload queue	   */
	for(i=0;i<5;i=i+1)
	{
		WorkloadMbox[i]  = OSMboxCreate((void*)0);		  /* Creation of Workload mailbox			   */
	}
	
    OSTaskCreate(TaskStart, (void *)0, &TaskStartStk[TASK_STK_SIZE - 1], 0);
    OSStart();                                             /* Start multitasking                       */
}

/*
*********************************************************************************************************
*                                              STARTUP TASK
*********************************************************************************************************
*/
void  TaskStart (void *pdata)
{
#if OS_CRITICAL_METHOD == 3                                /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr;
#endif
    char       s[100];
    INT16S     key;
	INT8U  err;

 
	Structure dat;

    pdata = pdata;                                         /* Prevent compiler warning                 */
 
    TaskStartDispInit();                                   /* Initialize the display                   */
 
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);                           /* Install uC/OS-II's clock tick ISR        */
    PC_SetTickRate(OS_TICKS_PER_SEC);                      /* Reprogram tick rate                      */
    OS_EXIT_CRITICAL();
 
    OSStatInit();                                          /* Initialize uC/OS-II's statistics         */
        
    TaskStartCreateTasks();                                /* Create all the application tasks         */
 
	dat.taskNumber=16;
	
    for (;;) {
        TaskStartDisp();                                  /* Update the display                       */


        if (PC_GetKey(&key) == TRUE) {                     /* See if key has been pressed              */
            if (key == 0x1B) {                             /* Yes, see if it's the ESCAPE key          */
                PC_DOSReturn();                            /* Return to DOS                            */
            }
        }

        OSCtxSwCtr = 0;                                    /* Clear context switch counter             */
        OSTimeDlyHMSM(0, 0, 1, 0);                         /* Wait one second                          */
		
		err=OSQPost(ToDisplay, (void*)&dat);				   /* Send message every one second -> for calculating delta */
		if(err!=OS_NO_ERR)
		{
			PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			PC_DispStr(0, 2, "Error in TaskStart OSQPost ToDisplay!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
		}
	}
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                        INITIALIZE THE DISPLAY
*********************************************************************************************************
*/

static  void  TaskStartDispInit (void)
{
/*                                1111111111222222222233333333334444444444555555555566666666667777777777 */
/*                      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
    PC_DispStr( 0,  0, "                         uC/OS-II, The Real-Time Kernel                         ", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
    PC_DispStr( 0,  1, "                                Jean J. Labrosse                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  2, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  3, "                                    EXAMPLE #1                                  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
	PC_DispStr( 0,  4, "Enter new workload:                                                                 ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  5, "Current workload:                                                               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  6, "Task|Counter|Workload   |Delta |                                                ", DISP_FGND_WHITE + DISP_BGND_BLUE);
    PC_DispStr( 0,  7, " Q1 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0,  8, " Q2 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0,  9, " Q3 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 10, " Q4 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 11, " Q5 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 12, " M1 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 13, " M2 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 14, " M3 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 15, " M4 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 16, " M5 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 17, " S1 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 18, " S2 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 19, " S3 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 20, " S4 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 21, " S5 |       |           |      |                                                ", DISP_FGND_BLACK + DISP_BGND_CYAN);
    PC_DispStr( 0, 22, "#Tasks          :        CPU Usage:     %                                       ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 23, "#Task switch/sec:                                                               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 24, "                            <-PRESS 'ESC' TO QUIT->                             ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
/*                                1111111111222222222233333333334444444444555555555566666666667777777777 */
/*                      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                           UPDATE THE DISPLAY
*********************************************************************************************************
*/
 
static  void  TaskStartDisp (void)
{
    char   s[80];
 
 
    sprintf(s, "%5d", OSTaskCtr);                                  /* Display #tasks running               */
    PC_DispStr(18, 22, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
 
#if OS_TASK_STAT_EN > 0
    sprintf(s, "%3d", OSCPUUsage);                                 /* Display CPU usage in %               */
    PC_DispStr(36, 22, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
#endif
 
    sprintf(s, "%5d", OSCtxSwCtr);                                 /* Display #context switches per second */
    PC_DispStr(18, 23, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
 
    sprintf(s, "V%1d.%02d", OSVersion() / 100, OSVersion() % 100); /* Display uC/OS-II's version number    */
    PC_DispStr(75, 24, s, DISP_FGND_YELLOW + DISP_BGND_BLUE);
 
    switch (_8087) {                                               /* Display whether FPU present          */
        case 0:
             PC_DispStr(71, 22, " NO  FPU ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 1:
             PC_DispStr(71, 22, " 8087 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 2:
             PC_DispStr(71, 22, "80287 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
 
        case 3:
             PC_DispStr(71, 22, "80387 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
    }
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                             CREATE TASKS
*********************************************************************************************************
*/

static  void  TaskStartCreateTasks (void)
{
    INT8U  i;
 
    OSTaskCreate(Keyboard, (void *)0, &KeyboardStk[TASK_STK_SIZE - 1], 1); /* Task handling keyboard */ 
    OSTaskCreate(Edit, (void *)0, &EditStk[TASK_STK_SIZE - 1], 2);   /* Task rewriting data from queue to mailbox */
    OSTaskCreate(Display, (void *)0, &DisplayStk[TASK_STK_SIZE - 1], 3);  /* Task displaying data on the screen   */
	OSTaskCreate(Send, (void*)0, &SendStk[TASK_STK_SIZE - 1], 4);  /* Task getting data from Edit task, changing WORKLOAD and sending it to Queue and Mailbox */
	
	for(i=1;i<=15;i++)
	{
		TaskData[i]=i;
	}

	for(i=1;i<=5;i++)
	{		
		OSTaskCreate(MboxTask, (void *)&TaskData[i], &TaskStk[i+3][TASK_STK_SIZE - 1], i+4); 
		OSTaskCreate(QTask, (void *)&TaskData[i+5], &TaskStk[i+8][TASK_STK_SIZE - 1], i+9);
		OSTaskCreate(SemTask, (void *)&TaskData[i+10], &TaskStk[i+13][TASK_STK_SIZE - 1], i+14);
    }
}

/*
*********************************************************************************************************
*                                         KEYBOARD HANDLING TASK
*********************************************************************************************************
*/

void Keyboard (void *pdata)
{
    INT16S key;
	INT8U  err;
 
	pdata = pdata;	                           /* Prevent compiler warning                 */											

	for(;;)
		{													
			if(PC_GetKey(&key) == TRUE)          /* See if key has been pressed              */
			{							
				OSQPost(ToEdit, (void *)&key);  /* Sends a message to a queue         */
				if(err!=OS_NO_ERR)
				{
					PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					if(err==OS_Q_FULL)
					{
						PC_DispStr(0, 2, "Error in Keyboard OSQPost ToEdit - OS_Q_FULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
					if(err==OS_ERR_EVENT_TYPE)
					{
						PC_DispStr(0, 2, "Error in Keyboard OSQPost ToEdit - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
					if(err==OS_ERR_PEVENT_NULL)
					{
						PC_DispStr(0, 2, "Error in Keyboard OSQPost ToEdit - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
				}
			}
			OSTimeDly(1);		                 /* Wait one tick                          */									
		}
}

/*
*********************************************************************************************************
*                   INTERPRETING DATA FROM QUEUE AND SENDING TO MAILBOX TASK
*********************************************************************************************************
*/

void Edit (void *pdata)	
{
	Structure dat;
	
	INT16S  *data;
    INT16S key;
	INT8U  err;
	
	INT8U xpos = 0;                                          /* Current x position on the screen  */																						
	INT8U i = 0;
	
    char toPrint[MSG_QUEUE_SIZE]=" ";                          /* Array of chars to be printed on the screen  */							
	
	INT32U wrkld = 0;
	
	pdata = pdata;		                   /* Prevent compiler warning                 */

	dat.taskNumber = 0;						/* Number 0 singaling that it is a message with data from Keyboard */
	dat.workload = 0;
   
    for (;;) 
	{	
	    data = OSQPend(ToEdit, 0, &err);         	        /* Receives a message from queue   */
		key = *data;

		if(err == OS_NO_ERR)                               /* Check if there has been an error  */
        {  
			if(key >= 0x30 && key <= 0x39)               /* See if key pressed is a number 0-9 */        
				{
					if(xpos < MSG_QUEUE_SIZE - 1)
					{
						toPrint[xpos] = key;
						xpos++;
					}
				}
				
			else if(key == 0x0D)                         /* See if key pressed is ENTER      */
			{	
				if(xpos >= 1)
                {									
					wrkld = strtoul(toPrint,(void *)&toPrint[10],10);	/* Converting array of chars to unsigned long */
					
					OSQPost(ToSend, (void *)&wrkld);					/* Sending workload to Send task */
					
					dat.workload = wrkld;								/* Assigning value to the structure, so that it can be sent to Display task */
					
					for(i=0; i<=MSG_QUEUE_SIZE+1; i++)
					{
						toPrint[i]=NULL;								/* Clearing the line */
					}
				}
				xpos = 0;
			}

			else if(*data == 0x08)                      /* See if key pressed is BACKSPACE      */
            {
				if(xpos >= 1)
                {
					toPrint[--xpos] = NULL;
				}
			}
			
			else if(*data == 0x7F)                   /* See if key pressed is DELETE            */
            {
				for(i = 0; i-1 < MSG_QUEUE_SIZE; i++)
                {		
					toPrint[i] = NULL;
				}
				xpos = 0;
			}
			
			else if (*data == 0x1B)                 /* See if key pressed is ESC                 */
            {
				PC_DOSReturn();		               /* Return to DOS                            */
			}
			
			for(i=0;i<=MSG_QUEUE_SIZE;i++)
			{
				dat.toPrint[i] = toPrint[i];
			}
			
			err=OSQPost(ToDisplay, (void *)&dat);		/* Sends a message to mailbox      */
			if(err!=OS_NO_ERR)
			{
				PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
				if(err==OS_Q_FULL)
				{
					PC_DispStr(0, 2, "Error in Edit OSQPost ToDisplay - OS_Q_FULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
				}
				if(err==OS_ERR_EVENT_TYPE)
				{
					PC_DispStr(0, 2, "Error in Edit OSQPost ToDisplay - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
				}
				if(err==OS_ERR_PEVENT_NULL)		
				{
					PC_DispStr(0, 2, "Error in Edit OSQPost ToDisplay - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
				}
			}
		}
		
		else if(err!=OS_NO_ERR)
		{
			PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			if(err==OS_TIMEOUT)
			{
				PC_DispStr(0, 2, "Error in Edit OSQPend ToEdit - OS_TIMEOUT!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_EVENT_TYPE)
			{
				PC_DispStr(0, 2, "Error in Edit OSQPend ToEdit - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_PEVENT_NULL)		
			{
				PC_DispStr(0, 2, "Error in Edit OSQPend ToEdit - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
		}
	}
}

/*
*********************************************************************************************************
*                                             DISPLAY TASK                                                                   
*********************************************************************************************************
*/

void Display (void *pdata)
{
	Structure *dat;
	
	INT8U  err;
	void  *data;

	INT8U i = 0;

	char currentWorkload[16]; 				/* Array of chars for printing current workload (number that was accepted with Enter) on the screen */
	char newWorkload[16];					/* Array of chars for printing new workload (numbers that are read from keyboard) on the screen */
	
	char cntr[16];							/* Array of chars for printing "counter" on the screen  */
	char taskWorkload[16];					/* Array of chars for printing "workload" of each tasks on the screen */
	char errors[80];						/* Array of chars containing error message */
	INT8U nr; 								/* Number of task */
	
	INT32U delta[15];						/* Array  containing Delta (counter_sec - counter_prevsec) of each task */
	INT32U counter_sec[15];					/* For calculating delta, value of counter in current sec */
	INT32U counter_prevsec[15];				/* For calculating delta, value of counter in previous sec */
	
	pdata = pdata;		                   /* Prevent compiler warning                 */

	PC_DispStr(0, 5, "Current workload:0", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);

	for(i=0;i<15;i++)
	{
		counter_prevsec[i]=0;				/* Initializing counter_prevsec, so that in first second it's equal 0 */
	}
    for (;;) 
    {
		dat = OSQPend(ToDisplay, 0, &err);		/* Receives a message from mailbox  */

		if(err == OS_NO_ERR)                               /* Check if there has been an error  */
        {			
			nr = dat->taskNumber;
			
			if (nr==0)									/* If number is 0, data from keyboard */
			{
				for(i=0;i<=15;i++)
				{
					newWorkload[i] = dat->toPrint[i];
				}
				PC_DispStr(19, 4,"                  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
				PC_DispStr(19, 4, newWorkload, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);	/* Displaying new workload on the screen */
				
				ultoa(dat->workload, currentWorkload,10);	/* Converting Workload from unsigned long to Char array */
				PC_DispClrRow(5, DISP_BGND_LIGHT_GRAY);			/* Clearing whole row, so it doesn't contain trash */
				PC_DispStr(0, 5, "Current workload:", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
				PC_DispStr(17, 5, currentWorkload, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);	/* Displaying current workload on the screen */
			}
			
			else if(nr>=1 && nr<=15)								/* If number is between 1 and 15, message from one of 15 workload tasks */
			{
				counter_sec[nr-1]=dat->counter;				/* Updating counter data for calculating delta */
				
				ultoa(dat->counter, cntr, 10);				/* Converting Counter from unsigned long to Char array */
				ultoa(dat->workload,taskWorkload,10);		/* Converting Workload from unsigned long to Char array */
				PC_DispStr(5, nr+6, cntr, DISP_FGND_BLACK + DISP_BGND_CYAN);	/* Displaying Counter on the screen */
				PC_DispStr(13, nr+6, "          ", DISP_FGND_BLACK + DISP_BGND_CYAN);	/* Clearing previous Workload */
				PC_DispStr(13, nr+6, taskWorkload, DISP_FGND_BLACK + DISP_BGND_CYAN);	/* Displaying Workload on the screen */
			}
			
			else if (nr==16)									/* If number is 16, message from TaskStart, meaning one second has passed */
			{
				for(i=0;i<15;i++)
				{
					delta[i]=counter_sec[i]-counter_prevsec[i];	/* Calculating delta of each task */
					counter_prevsec[i]=counter_sec[i];			/* Counter_prevsec is assigned data from counter_sec, for further use */
					ultoa(delta[i], cntr,10);					/* Converting delta to array of chars */
					PC_DispStr(26, i+7, "    ", DISP_FGND_BLACK + DISP_BGND_CYAN);
					PC_DispStr(26, i+7, cntr, DISP_FGND_BLACK + DISP_BGND_CYAN);	/*Displaying delta on the screen */
				}
			}
		}
		else if(err!=OS_NO_ERR)
		{
			PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			if(err==OS_TIMEOUT)
			{
				PC_DispStr(0, 2, "Error in Display OSQPend ToDisplay - OS_TIMEOUT!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_EVENT_TYPE)
			{
				PC_DispStr(0, 2, "Error in Display OSQPend ToDisplay - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_PEVENT_NULL)		
			{
				PC_DispStr(0, 2, "Error in Display OSQPend ToDisplay - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
		}
	}
}

/*
*********************************************************************************************************
*							SENDING INFO ABOUT WORKLOAD TO TASKS
*********************************************************************************************************
*/

void Send (void *pdata)	
{				        
    INT8U  err;
    INT32U *data;
	
    INT32U wrkld;
	Structure dat;
	Structure *msg;
	INT32U *toSend;
    INT8U i = 0;	
    
	pdata = pdata;		                   /* Prevent compiler warning                 */
    
    for (;;) 
	{	
		data = OSQPend(ToSend, 0, &err);	/* Receiving data from queue from Edit task */
		
		if (err==OS_NO_ERR)
		{
			wrkld = *data;

			for(i=0;i<TSKS_GROUP;i++)
			{
				err=OSQPost(WorkloadQueue, (void *)&wrkld);			/* Sending new workload for Queue Tasks */
				if(err!=OS_NO_ERR)
				{
					PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					if(err==OS_Q_FULL)
					{
						PC_DispStr(0, 2, "Error in Send OSQPost WorkloadQueue - OS_Q_FULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
					if(err==OS_ERR_EVENT_TYPE)
					{
						PC_DispStr(0, 2, "Error in Send OSQPost WorkloadQueue - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
					if(err==OS_ERR_PEVENT_NULL)		
					{
						PC_DispStr(0, 2, "Error in Send OSQPost WorkloadQueue - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
				}
			}
			
			OSSemPend(WorkloadSem, 0, &err);	/* Acquire semaphore for global variable */
			if (err==OS_NO_ERR)
			{
				WORKLOAD = wrkld;					/* Update global variable 				 */
				OSSemPost(WorkloadSem); 			/* Release semaphore					 */
			}
			else if(err!=OS_NO_ERR)
			{
				PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
				if(err==OS_TIMEOUT)
				{
					PC_DispStr(0, 2, "Error in Send OSSemPend WorkloadSem - OS_TIMEOUT!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
				}
				if(err==OS_ERR_EVENT_TYPE)
				{
					PC_DispStr(0, 2, "Error in Send OSSemPend WorkloadSem - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
				}
				if(err==OS_ERR_PEVENT_NULL)		
				{
					PC_DispStr(0, 2, "Error in Send OSSemPend WorkloadSem - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
				}
			}
			
			for(i=0;i<TSKS_GROUP;i++)
			{
				err = OSMboxPost(WorkloadMbox[i], (void *)&wrkld);
				if(err!=OS_NO_ERR)
				{
					PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					if(err==OS_MBOX_FULL)
					{
						PC_DispStr(0, 2, "Error in Send OSMboxPost WorkloadMbox - OS_MBOX_FULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
					if(err==OS_ERR_EVENT_TYPE)
					{
						PC_DispStr(0, 2, "Error in Send OSMboxPost WorkloadMbox - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
					if(err==OS_ERR_PEVENT_NULL)		
					{
						PC_DispStr(0, 2, "Error in Send OSMboxPost WorkloadMbox - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
					if(err==OS_ERR_POST_NULL_PTR)
					{
						PC_DispStr(0, 2, "Error in Send OSMboxPost WorkloadMbox - OS_ERR_POST_NULL_PTR!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
					}
				}
			}
		}
		else if(err!=OS_NO_ERR)
		{
			PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			if(err==OS_TIMEOUT)
			{
				PC_DispStr(0, 2, "Error in Send OSQPend ToSend - OS_TIMEOUT!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_EVENT_TYPE)
			{
				PC_DispStr(0, 2, "Error in Send OSQPend ToSend - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_PEVENT_NULL)		
			{
				PC_DispStr(0, 2, "Error in Send OSQPend ToSend - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
		}
    }
}

/*
*********************************************************************************************************
*                                        QUEUE TASKS
*********************************************************************************************************
*/

void QTask(void *pdata)
{
	INT8U p=*(INT8U *)pdata;

	Structure dat;
	
	Structure tmp;

	INT32U counter = 0;
	INT32U workload = DEFAULT_WORKLOAD;
	INT8U  err;
	
	INT32U i;
	INT32U x = 0;

	void *msg;
	
	dat.taskNumber = p;
	dat.workload = 0;
	
	for(;;)
	{
		msg = OSQAccept(WorkloadQueue);				/* Receiving message from Queue */
		if(msg!=(void *)0)
		{
			workload = *(INT32U *)msg;
		}
		
		dat.counter = counter;
		dat.workload = workload;
	
		err=OSQPost(ToDisplay, (void*)&dat);				/* Sending data to Display via Mailbox */
		if(err!=OS_NO_ERR)
		{
			PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			if(err==OS_Q_FULL)
			{
				PC_DispStr(0, 2, "Error in QTask OSQPost ToDisplay - OS_Q_FULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_EVENT_TYPE)
			{
				PC_DispStr(0, 2, "Error in  QTask OSQPost ToDisplay - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_PEVENT_NULL)		
			{
				PC_DispStr(0, 2, "Error in QTask OSQPost ToDisplay - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
		}
		
		for (i=0;i<workload;i++)						/* Workload loop 						*/
		{
			x++;
		}
		
		counter=1;										/* Counter incrementation 					*/
		
		OSTimeDly(1);									/* Wait one tick                          */
	}
}

/*
*********************************************************************************************************
*                                        MAILBOX TASKS
*********************************************************************************************************
*/

void MboxTask(void *pdata) 
{
	INT8U p=*(INT8U *)pdata;

	Structure dat;

	INT32U counter = 0;
	INT32U workload = DEFAULT_WORKLOAD;
	INT8U  err;
	
	INT32U i;
	INT32U x = 0;
	
	void *msg;
	
	dat.taskNumber = p;
	dat.workload = 0;
	
	for(;;)
	{ 
		msg = OSMboxAccept(WorkloadMbox[p-1]);				/* Receiving message from Messagebox */
		
		if(msg!=(void *)0)
		{
			workload = *(INT32U *)msg;
		}
		
		dat.counter = counter;
		dat.workload = workload;
	
		err=OSQPost(ToDisplay, (void*)&dat);				/* Sending data to Display via Mailbox */
		if(err!=OS_NO_ERR)
		{
			PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			if(err==OS_Q_FULL)
			{
				PC_DispStr(0, 2, "Error in MboxTask OSQPost ToDisplay - OS_Q_FULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_EVENT_TYPE)
			{
				PC_DispStr(0, 2, "Error in  MboxTask OSQPost ToDisplay - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_PEVENT_NULL)		
			{
				PC_DispStr(0, 2, "Error in MboxTask OSQPost ToDisplay - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
		}
		
		for (i=0;i<workload;i++)						/* Workload loop 						*/
		{
			x++;
		}
		
		counter++;										/* Counter incrementation 					*/
		
		OSTimeDly(1);									/* Wait one tick                          */
	}
}

/*
*********************************************************************************************************
*                                        SEMAPHORE TASKS
*********************************************************************************************************
*/
void SemTask(void *pdata) 
{
	INT8U p=*(INT8U *)pdata;
	
	Structure dat;

	INT32U counter = 0;
	INT32U workload = DEFAULT_WORKLOAD;
	INT8U  err;
	
	INT32U i;
	INT32U x = 0;
	
	dat.taskNumber = p;
	dat.workload = 0;
	
	for(;;)
	{
		OSSemPend(WorkloadSem, 0, &err);
		if (err==OS_NO_ERR)
		{
			workload = WORKLOAD;
			OSSemPost(WorkloadSem);
		}
		else if(err!=OS_NO_ERR)
		{
			PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			if(err==OS_TIMEOUT)
			{
				PC_DispStr(0, 2, "Error in Send OSSemPend WorkloadSem - OS_TIMEOUT!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_EVENT_TYPE)
			{
				PC_DispStr(0, 2, "Error in Send OSSemPend WorkloadSem - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_PEVENT_NULL)		
			{
				PC_DispStr(0, 2, "Error in Send OSSemPend WorkloadSem - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
		}

		dat.counter = counter;
		dat.workload = workload;
		
		err=OSQPost(ToDisplay, (void*)&dat);				/* Sending data to Display via Mailbox */
		if(err!=OS_NO_ERR)
		{
			PC_DispStr(0, 2, "                                                                      ", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			if(err==OS_Q_FULL)
			{
				PC_DispStr(0, 2, "Error in SemTask OSQPost ToDisplay - OS_Q_FULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_EVENT_TYPE)
			{
				PC_DispStr(0, 2, "Error in  SemTask OSQPost ToDisplay - OS_ERR_EVENT_TYPE!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
			if(err==OS_ERR_PEVENT_NULL)		
			{
				PC_DispStr(0, 2, "Error in SemTask OSQPost ToDisplay - OS_ERR_PEVENT_NULL!", DISP_FGND_RED + DISP_BGND_LIGHT_GRAY);
			}
		}
		
		for (i=0;i<workload;i++)						/* Workload loop 						*/
		{
			x++;
		}
		
		counter++;										/* Counter incrementation 					*/
		
		OSTimeDly(1);									/* Wait one tick                          */
	}
}