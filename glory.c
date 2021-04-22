//***********************************************************************************************
//***********************************************************************************************
// File:   glory.c
// Author: Jonathan Orrego
// V2.0.0_26/11/2019: Versi�n inicial.
// V2.0.1_24/01/2020: Le alargu� el tiempo de movimiento al pist�n de J21 medido por Timer1.
// V2.3 Se modifico los sensores de la bolsa para 2 sensores J13 y J6
//***********************************************************************************************
//***********************************************************************************************

// BAG INPLACE
// BAG REMOVED
// BAG APROVED


#include "glory_header.h"   //Header con los configuration bits.
#include <string.h>         //Menejo de strings.
#include <stdio.h>
#include <stdlib.h>
#define _XTAL_FREQ 8000000  //Para que anden los __delay_ms()

//Definiciones...
void USART_Init(long);
void USART_Envia(char);
void USART_EnviaMsg(char *str);
void __interrupt() isr(void);

#define F_CPU 8000000/16 //Si es 16 luego poner BRGH=1, Si es 64 luego poner BRGH=0. Para 38400bps conviene poner 16. El reloj interno llega a 8Mhz en este micro.

char CRLF[] = "\x0D\x0A";
//Mensaje de Ayuda...
char MsgAyuda[] = "Press O open / C close / S status / U UnLock door / E clear errors / B print bag mode / V version / H help";

//Strings en variables para no repetirlos...
char MsgCritical_0[] = "CRITICAL : BAG BAG_STATE_REMOVED";
char MsgCritical_1[] = "CRITICAL : BAG BAG_STATE_PUTTING";
char MsgCritical_2[] = "CRITICAL : BAG BAG_STATE_TAKING";
char MsgCritical_3[] = "CRITICAL : BAG BAG_STATE_IN_PLACE";

int ValorStateE;
int ValorStateShutter;  //Inicio:0. Inicio cierre: 3. Cerrado: 9. Inicio apertura: 2. Abierto: 8.
int ValorStateLock;     //0: Locked. 1: Unlocked.
int ValorBagSensor;
int ValorBagStatus;
int Counter;            //0: Counter in place. 1: Counter removed 1. 2: Counter removed 2. 3: Counter removed 3.

int PuertaAbierta;

int ValorBag;               //Estado de secuencia de la bolsa.
int EstadoAprobacionBolsa;  //Estado de aprobaci�n de la bolsa:  0=No aprobada 1=Aprobada.

int PulsoArriba;            //0:Abajo, 1:Arriba.
int TiempoPulso;
int PulsoArribaBuzzer;      //0:Abajo, 1:Arriba.
int TiempoPulsoBuzzer;

int ModoTrabajo;
int Counter;

//Mensaje de Version...
char MsgVersion[] = "Version : glory_2.3.1";
//Mensaje de Personalidad...
char MsgPersonalidad1[] = "bag mode GLORY glory_2.01";
char MsgPersonalidad2[] = "new bag mode GLORY glory_2.1";

//Estado de sensores ya debounceado...
int J1, J2, J3, J4, J5, J6, J7, J8, J9, J10, J11, J12, J13, J14;

int StartUp; //Para ejecutar c�digo apenas inicia y nunca m�s.

//Uso general...
char txt[5];        //Para hacer conversiones...
int Tiempo0;        //Para medir el tiempo de la puerta abierta.
int Tiempo1;        //Para medir el tiempo de la exclusa m�vil.
int Tiempo2;        //Para medir el tiempo del buzzer.

int DelaySensor = 20;    //Delay para verificar antirebote

//Modos de trabajo
enum Mode{
    Indirect,
    Direct
};

//***********************************************************************************************
//***********************************************************************************************
//***********************************************************************************************
//***********************************************************************************************

void main(void) {
    int i;

    //Configuraci�n del micro...
    OSCCON=0b01110010;      //Oscilador interno, 8MHZ.
    ADCON1=0b00001111;      //Todos los pines digitales.
    TRISA=0b00000000;       //PortA salidas.
    TRISB=0b11111111;       //PortB todo en entrada.
    TRISC=0b10010000;       //PortC entrada serial y SPI, el resto salidas.
    TRISD=0b11111111;       //PortD todo en entrada.
    TRISE=0b00000000;       //PortE salidas.
    //Configuraci�n del Timer0 para interrupci�n cada 100ms...
    T0CON=0x81;
    TMR0H=0x3C; //Valor precargado.
    TMR0L=0xB0; //Valor precargado.
    TMR0IF=0;       //Limpio flag.
    TMR0IE=0;       //Apago la interrupci�n del Timer0.
    //Configuraci�n del Timer1 para interrupci�n cada 10ms...
    T1CON=0x01; // Set the timer to prescaler 1:1 (fastest), clock source instruction clock
                  //    and pick timer2 clock in as source (not Secondary Oscillator)
                  //    we also select synchronization, no effect as we run of FOSC here.
    TMR1H=0xB1; //Valor precargado.
    TMR1L=0xE0; //Valor precargado.
    //Configuraci�n del Timer2 para interrupci�n cada 10ms...  (error de 25us)
    T2CON=0x26;
    PR2=250;
    
    //Apago salidas...
    LATA2=0;
    LATC1=0;
    LATC2=0;

    //Inicializaci�n de variables...
    J1=1;   //J1 - Sin uso (Personalidad).
    J2=1;   //J2 - Exclusa.
    J3=1;   //J3 - Sin uso.
    J4=1;   //J4 - Sin uso.
    J5=1;   //J5 - Bolsa A.
    J6=1;   //J6 - Bolsa B.
    J7=1;   //J7 - Sin uso.
    J8=1;   //J8 - Puerta.
    J9=1;   //J9 - Exclusa.
    J10=1;  //J10 - Sin uso.
    J11=1;  //J11 - Sin uso.
    J12=1;  //J12 - Bolsa L.
    J13=1;  //J13 - Bolsa C.
    J14=1;  //J14 - Sin uso.
    J6=PORTDbits.RD3;
    J13=PORTDbits.RD2; 
            
    StartUp=1;
    ValorStateE=0;
    ValorStateShutter=0;        //Observado en la placa inicia en 0.
    ValorStateLock=0;           //Cerradura.
    ValorBagSensor=0;
    ValorBagStatus=0;
    ValorBag=99;                //Valor inicial que se va a Bag Inplace o a Error.
    EstadoAprobacionBolsa=0;    //Por defecto no aprobada.
    Counter=0;
    
    PulsoArriba=0;
    TiempoPulso=0;
    PulsoArribaBuzzer=0;
    TiempoPulsoBuzzer=0;
    
    //Inicializa la USART...
    USART_Init(38400);
    //Configuro interrupciones...
    GIE = 1;        // Activo interrupciones globales.
    PEIE = 1;       // Activo interrupciones perif�ricos.
    RCIE = 1;       // Habilito la interrupci�n de RX USART.

    //Simulo los puntitos...
    for(i=0;i<=50;i++)
    {
        USART_EnviaMsg(".");
        __delay_ms(30);
    }    
    //Mensaje al encender la Placa
   USART_EnviaMsg(" ");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg("   IOBOARD SCREW !!!");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg("********************************");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg("  -By PERMAQUIM SPA");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg(" ");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg("  -For BRINKS CHILE");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg(" ");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg("   -Version: 2.3.1");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg("********************************");
   USART_EnviaMsg(CRLF);
   USART_EnviaMsg(" ");
   USART_EnviaMsg(CRLF);
   __delay_ms(30);
   
   
    //Cierro la exclusa...
    ValorStateShutter=3;   //Inicia cierre. 
    LATE0=1;    //J21 Pin1 alto.
    LATA5=0;    //J21 Pin2 bajo.
    LATC1=1;    //Enable.
    Tiempo1=0;
    TMR1H=0xB1;     //Precargo.
    TMR1L=0xE0;     //Precargo.
    TMR1IF=0;       //Limpio flag.
    TMR1IE=1;       //Prendo la interrupci�n del Timer1.
    USART_EnviaMsg("START CLOSE");
    USART_EnviaMsg(CRLF);
    USART_EnviaMsg("WARNING CLOSE SENSOR DISABLED");
    USART_EnviaMsg(CRLF);
    ModoTrabajo = 0;
   /* if ((PORTBbits.RB5==0) ) //J1 Si J1 tiene Jumper se configura modo directo    && (J1==1)
        {
            ModoTrabajo = 1;
            USART_EnviaMsg("ModoDirecto configurado");
            USART_EnviaMsg(CRLF);
        }
    else {
        USART_EnviaMsg("Modo indirecto");
        USART_EnviaMsg(CRLF);
    }*/
    while(1)
    {
            //Lectura de entradas...
            //Puerta - J8...
            if ((PORTBbits.RB4 == 0) && (J8 == 1)) //J8
            {
                __delay_ms(100);         //Antibounce.
                if (PORTBbits.RB4 == 0) //Vuelvo a verificar que est� presionado.
                {
                    J8 = 0;
                }
            }
            if (PORTBbits.RB4 == 1)
                J8 = 1;
            //Bolsa B - J6...
            if ((PORTDbits.RD3 == 0) && (J6 == 1)) //J6
            {
                __delay_ms(100);         //Antibounce.
                if (PORTDbits.RD3 == 0) //Vuelvo a verificar que est� presionado.
                {
                    J6 = 0;
                }
            }
             if (PORTDbits.RD3 == 1)
               J6 = 1;
            //Bolsa A - J13...
            if ((PORTDbits.RD2 == 0) && (J13 == 1)) //J13
            {
                __delay_ms(100);         //Antibounce.
                if (PORTDbits.RD2 == 0) //Vuelvo a verificar que est� presionado.
                {
                    J13 = 0;
                }
            }
            if (PORTDbits.RD2 == 1)
                J13 = 1;
            
            //Detecto puerta abierta...
            if ((PuertaAbierta == 0 && J8 == 1) || (StartUp == 1 && J8 == 1))
            {
                PuertaAbierta = 1;
                USART_EnviaMsg("CRITICAL: door open");
                USART_EnviaMsg(CRLF);
                if (TMR0IE == 1) //Si est� contando el tiempo de 10 segs.
                {
                    ValorStateLock = 0;
                    TMR0IE = 0; //Apago el Timer0.
                    LATA2 = 0;  //Disable.
                    LATA1 = 0;  //Pin1 bajo.
                    LATA0 = 0;  //Pin2 bajo.
                    USART_EnviaMsg("CRITICAL: door locked");
                    USART_EnviaMsg(CRLF);
                }
            }
            //Detecto puerta cerrada...
            if ((PuertaAbierta == 1 && J8 == 0) || (StartUp == 1 && J8 == 0))
            {
                PuertaAbierta = 0;
                USART_EnviaMsg("CRITICAL: door closed");
                USART_EnviaMsg(CRLF);
                if (TMR0IE == 1) //Si est� contando el tiempo de 10 segs.
                {
                    ValorStateLock = 0;
                    TMR0IE = 0; //Apago el Timer0.
                    LATA2 = 0;  //Disable.
                    LATA1 = 0;  //Pin1 bajo.
                    LATA0 = 0;  //Pin2 bajo.
                    USART_EnviaMsg("CRITICAL: door locked");
                    USART_EnviaMsg(CRLF);
                }
            }
            
        

        if (ModoTrabajo == 0)   // Si modoDirect es false, usamos modo 4 sensores para detectar bolsa
        {
            //Armo State Gate E...
            ValorStateE = (J8 * 4) + (J9 * 2) + J2;
            //Armo Bag_Sensor...
            ValorBagSensor = (J6 * 2) + J13; 
            //Armo ValorBagStatus...
            if (J6 == 1 && J13 == 1) //J5 == 1 &&
                ValorBagStatus = 2;
            if (J6 == 0 && J13 == 1) //J5 == 0 && 
                ValorBagStatus = 0;
            if (J6 == 0 && J13 == 0) //J5 == 0 && 
                ValorBagStatus = 1;
            if (J6 == 1 && J13 == 0) //J5 == 0 && 
                ValorBagStatus = 0;
     
            //Detecto estado de la bolsa...
       // do{
        //if (ValorBag == 99) //Estado inicial al encender.
            
            //Bolsa B - J6...
       if ((PORTDbits.RD3 == 0) && (J6 == 1)) //J6
        {
                __delay_ms(100);         //Antibounce.
       if (PORTDbits.RD3 == 0) //Vuelvo a verificar que est� presionado.
            {
            J6 = 0;
            }
        }
      if (PORTDbits.RD3 == 1)
            J6 = 1;
            //Bolsa A - J13...
      if ((PORTDbits.RD2 == 0) && (J13 == 1)) //J13
        {
                __delay_ms(100);         //Antibounce.
      if (PORTDbits.RD2 == 0) //Vuelvo a verificar que est� presionado.
            {
            J13 = 0;
            }
        }
      if (PORTDbits.RD2 == 1)
                J13 = 1;
       
       
      if (J13 == 0 && J6 == 0){
          ValorBag = 2;
          //USART_EnviaMsg(MsgCritical_3);
          //USART_EnviaMsg(CRLF);
      }
      if (J13 == 0 && J6 == 1){
          ValorBag = 0;
          //USART_EnviaMsg("BAG PUTTING");
          //USART_EnviaMsg(CRLF);
      }
      if (J13 == 1 && J6 == 0){
          ValorBag = 0;
          //USART_EnviaMsg("BAG ERROR");
         // USART_EnviaMsg(CRLF);
      }
      if ((J13 && J6)== 1){
          ValorBag = 1;
          //USART_EnviaMsg("BAG REMOVED");
          //USART_EnviaMsg(CRLF);
      }
     
            
            
            
                           
            //Buzzer...
            if ((ValorBag != 2 || J6 != 0) && (ValorStateLock == 0)) //Para que no suene debe estar BAG INPLACE y el J6 puesto (el comando U tambi�n lo hace sonar).
            {
                LATC2 = 1;  //Enable.
                TMR2IF = 0; //Limpio flag.
                TMR2IE = 1; //Prendo la interrupci�n del Timer2.
            }
            else
            {
                if (ValorStateLock == 1) //Con la cerradura unlocked suena todo el tiempo.
                {
                    LATC2 = 1;  //Enable.
                    LATE2 = 1;  //Pin 1 arriba.
                    LATE1 = 0;  //Pin 2 abajo.
                    TMR2IF = 0; //Limpio flag.
                    TMR2IE = 0; //Apago la interrupci�n del Timer2.
                }
                else
                {
                    LATC2 = 0;  //Disable.
                    LATE2 = 0;  //Pin 1 abajo.
                    LATE1 = 0;  //Pin 2 abajo.
                    TMR2IE = 0; //Apago la interrupci�n del Timer2.
                }
            }
            StartUp = 0;
      
            
        } 
    }   
}

//***********************************************************************************************
//***********************************************************************************************
//***********************************************************************************************
//***********************************************************************************************
void USART_Init(long baud_rate)
{   
    float temp;
    temp=(((float)(F_CPU)/(float)baud_rate)-1); //Calculo del baud rate.
    SPBRG=(int)temp;                            //Redondeo a numero entero.
    //TXSTA=0x20;                               //Transmit Enable(TX) enable, BRGH=0.
    TXSTA=0x24;                                 //Transmit Enable(TX) enable, BRGH=1.
    RCSTA=0x90;                                 //Receive Enable(RX), serial port enable.
}
//***********************************************************************************************

void __interrupt() isr (void)
{
    char letrain;
    int n;
    
   
    //Interrupci�n por RX...
    if((PIR1bits.RCIF == 1) && (PIE1bits.RCIE == 1))
    {
        RCIF = 0; //Limpio flag.
        letrain=RCREG;
    
    
        switch(letrain)
        {
            case 'C': case 'c':
                if (ValorStateShutter==8)
                {
                    ValorStateShutter=3;   //Inicia cierre. 
                    LATE0=1;    //J21 Pin1 alto.
                    LATA5=0;    //J21 Pin2 bajo.
                    LATC1=1;    //Enable.
                    Tiempo1=0;
                    TMR1H=0xB1;     //Precargo.
                    TMR1L=0xE0;     //Precargo.
                    TMR1IF=0;       //Limpio flag.
                    TMR1IE=1;       //Prendo la interrupci�n del Timer1.
                    USART_EnviaMsg("START CLOSE");
                    USART_EnviaMsg(CRLF);
                    USART_EnviaMsg("WARNING CLOSE SENSOR DISABLED");
                    USART_EnviaMsg(CRLF);
                }
                break;    
            case 'O': case 'o':
                if (ValorStateShutter==9)
                {
                    ValorStateShutter=2;   //Inicia apertura. 
                    LATE0=0;    //J21 Pin1 bajo.
                    LATA5=1;    //J21 Pin2 alto.
                    LATC1=1;    //Enable.
                    Tiempo1=0;
                    TMR1H=0xB1;     //Precargo.
                    TMR1L=0xE0;     //Precargo.
                    TMR1IF=0;       //Limpio flag.
                    TMR1IE=1;       //Prendo la interrupci�n del Timer1.
                    USART_EnviaMsg("START OPEN");
                    USART_EnviaMsg(CRLF);
                    USART_EnviaMsg("WARNING OPEN SENSOR DISABLED");
                    USART_EnviaMsg(CRLF);
                }
                break;    
            case 'U': case 'u':
                if (ValorStateLock==0)
                {
                    ValorStateLock=1;
                    //Activo la salida...
                    LATA4=1;        //J18 Pin2 alto.
                    LATA3=0;        //J18 Pin1 bajo.
                    LATA1=1;        //J19 Pin1 alto.
                    LATA0=0;        //J19 Pin2 bajo.
                    LATA2=1;        //Enable J18 y J19.
                    PulsoArriba=1;  //J18 empieza arriba.
                    TiempoPulso=0;  //Conteo del tiempo del pulso J18.
                    //Empiezo a contar el tiempo.
                    Tiempo0=0;       
                    TMR0IE=1;   //Prendo la interrupci�n del Timer0.
                    USART_EnviaMsg("CRITICAL: door unlocked");
                    USART_EnviaMsg(CRLF);
                }
                break;    
            case 'E': case 'e':
                if(ModoTrabajo = 0){
                    if (J6==0 && J13==0)
                    {
                        ValorBag=2; 
                    }
                    else{
                        ValorBag=0;
                        USART_EnviaMsg("WARNING : BAG IN IT");
                        USART_EnviaMsg(CRLF);
                    }
                }
                
                break;
            case 'A': case 'a':
                if (ValorBag==2) //Si est� Bag Inplace.
                {
                    EstadoAprobacionBolsa=1;
                }else{
                    ValorBag=0;
                    USART_EnviaMsg("CRITICAL : BAG IS NOT IN PLACE CAN'T APROVE");
                    USART_EnviaMsg(CRLF);
                }
                break;
            case 'V': case 'v':
                USART_EnviaMsg(MsgVersion);
                USART_EnviaMsg(CRLF);
                break;
            case 'B': case 'b':
                if(ModoTrabajo == 0){
                    USART_EnviaMsg(MsgPersonalidad1);
                    USART_EnviaMsg(CRLF);
                }else{
                    USART_EnviaMsg(MsgPersonalidad2);
                    USART_EnviaMsg(CRLF);
                }
                break;
            case 'S': case 's':
                USART_EnviaMsg("STATE : BAG ");
                n=sprintf(txt,"%02u",ValorBag); //Convierto a string el ValorBag.
                USART_EnviaMsg(txt);
                /*USART_EnviaMsg(" BAG_APROVED ");
                n=sprintf(txt,"%01u",EstadoAprobacionBolsa); //Convierto a string el ValorBag.
                USART_EnviaMsg(txt);*/
                USART_EnviaMsg(" SHUTTER ");
                n=sprintf(txt,"%02X",ValorStateShutter); //Convierto a hexa el shutter.
                USART_EnviaMsg(txt);
                USART_EnviaMsg(" LOCK ");
                n=sprintf(txt,"%01u",ValorStateLock); //Convierto a hexa el shutter.
                USART_EnviaMsg(txt);
                USART_EnviaMsg(" GATE ");
                n=sprintf(txt,"%01u",ValorStateE); //Convierto a string el State E.
                USART_EnviaMsg(txt);
                USART_EnviaMsg(CRLF);
                break;
            default:
                USART_EnviaMsg(MsgAyuda);
                USART_EnviaMsg(CRLF);
                break;
        }
    }
    
    //Interrupci�n por timer 0...
    if (TMR0IF && TMR0IE)
    { 
        TMR0IF=0;       //Limpio flag.
        TMR0H=0x3C;     //Vuelvo a precargar.
        TMR0L=0xB0;     //Vuelvo a precargar.
        Tiempo0=Tiempo0+100;
            
        //Salida J18 alterna 1.2segs arriba y 0.4segs. abajo...
        if (PulsoArriba==1)
        {
            LATA4=1;    //J18 Pin2 alto.
            LATA3=0;    //J18 Pin1 bajo.
            TiempoPulso=TiempoPulso+100;
            if (TiempoPulso>=1200)
            {
                PulsoArriba=0;
                TiempoPulso=0;
            }
        }else{
            LATA4=0;    //J18 Pin2 bajo.
            LATA3=0;    //J18 Pin1 bajo.
            TiempoPulso=TiempoPulso+100;
            if (TiempoPulso>=400)
            {
                PulsoArriba=1;
                TiempoPulso=0;
            }
        }
        if (Tiempo0>=10500)   //La placa acciona 10.5segs
        {
            ValorStateLock=0;
            Tiempo0=0;
            TMR0IE=0;   //Apago el Timer0.
            LATA2=0;    //Disable.
            //J19...
            LATA1=0;    //J19 Pin1 bajo.
            LATA0=0;    //J19 Pin2 bajo.
            //J18...
            LATA4=0;    //J18 Pin2 bajo.
            LATA3=0;    //J18 Pin1 bajo.
            USART_EnviaMsg("CRITICAL: timeout waiting for door, locked");
            USART_EnviaMsg(CRLF);
        }
    }
    //Interrupci�n por timer 1...
    if (TMR1IF && TMR1IE)
    { 
        TMR1IF=0;       //Limpio flag.
        TMR1H=0xB1;     //Vuelvo a precargar.
        TMR1L=0xE0;     //Vuelvo a precargar.
        Tiempo1=Tiempo1+10;
        

        if (Tiempo1>=2020)   //La placa acciona 1.72segs 2050
        //if (Tiempo1>=3500)   //Acciono 3,5 segs siguiendo el video que mando Sebas que parece tardar algo de 2.5
        {
            Tiempo1=0;
            TMR1IE=0;   //Apago el Timer1.
            LATC1=0;    //Disable.
            //J21...
            LATE0=0;    //J21 Pin1 bajo.
            LATA5=0;    //J21 Pin2 bajo.
            if (ValorStateShutter==3)
            {
                USART_EnviaMsg("CLOSED");
                USART_EnviaMsg(CRLF);
                ValorStateShutter=9;   //Cerrado. 
            }
            if (ValorStateShutter==2)
            {    
                USART_EnviaMsg("OPEN");
                USART_EnviaMsg(CRLF);
                ValorStateShutter=8;   //Abierto. 
            }    
        }
    }
    
    //Interrupci�n por timer 2... (se prende o apaga por estado de J y comando U)
    if (TMR2IF && TMR2IE)
    { 
        TMR2IF=0;       //Limpio flag.
        Tiempo2=Tiempo2+10;
        if (PulsoArribaBuzzer==1)
        {
            LATE2=1;        //Pin 1 arriba.
            LATE1=0;        //Pin 2 abajo.
            TiempoPulsoBuzzer=TiempoPulsoBuzzer+10;
            if (TiempoPulsoBuzzer>=140)     //140ms arriba.
            {
                PulsoArribaBuzzer=0;
                TiempoPulsoBuzzer=0;
            }
        }else{
            LATE2=0;        //Pin 1 abajo.
            LATE1=0;        //Pin 2 abajo.
            TiempoPulsoBuzzer=TiempoPulsoBuzzer+10;
            if (TiempoPulsoBuzzer>=1920)    //1920ms abajo.
            {
                PulsoArribaBuzzer=1;
                TiempoPulsoBuzzer=0;
            }
        }
    }    
}

void USART_EnviaMsg (char *msgout)
{
    char letraout;
    while(*msgout != 0x00)      //Mientras no me tope con el null del final del string.
    {
        letraout=*msgout;       //Lee la letra del puntero.
        msgout++;               //Avanza puntero.
        while (TXIF==0){}       //Espera poder enviar.
        TXREG=letraout;         //Env�a la letra.
    }                
}


