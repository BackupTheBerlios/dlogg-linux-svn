/*****************************************************************************
 * Daten der UVR1611 lesen vom Datenlogger und im Winsol-Format speichern    *
 * read data of UVR1611 from Datanlogger and save it in format of Winsol     *
 * (c) 2006 - 2010 H. Roemer / C. Dolainsky  / S. Lechner                    *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU General Public License               *
 * as published by the Free Software Foundation; either version 2            *
 * of the License, or (at your option) any later version.                    *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program; if not, see <http://www.gnu.org/licenses/>.      *
 *                                                                           *
 * Version 0.1        18.04.2006 erste Testversion                             *
 * Version 0.5a        05.10.2006 Protokoll-Log in /var/dl-lesenx.log speichern *
 * Version 0.6        27.01.2006 C. Dolainsky                                  *
 *                     28.01.2006 Anpassung an geaenderte dl-lesen.h.           *
 *                     18.03.2007 IP                                            *
 * Version 0.7        01.04.2007                                               *
 * Version 0.7.7    26.12.2007 UVR61-3                                       *
 * Version 0.8        13.01.2008 2DL-Modus                                     *
 * Version 0.8.1     04.12.2009 --dir Parameter aufgenommen                   *
 * Version ?????    ??.01.2010 CAN-Logging                                   *
 *****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../dl-lesen.h"

//#define DEBUG 4

#define BAUDRATE B115200
#define UVR61_3 0x5A
#define UVR1611 0x76

extern char *optarg;
extern int optind, opterr, optopt;

int do_cleanup(void);
int check_arg(int arg_c, char *arg_v[]);
int check_arg_getopt(int arg_c, char *arg_v[]);
int erzeugeLogfileName(UCHAR ds_monat, UCHAR ds_jahr);
int erzeugeLogfileName_CAN(UCHAR ds_monat, UCHAR ds_jahr, int anzahl_Rahmen);
int open_logfile(char LogFile[], int geraet);
int open_logfile_CAN(char LogFile[], int datenrahmen);
int close_logfile(void);
int get_modulmodus(void);
int kopfsatzlesen(void);
void testfunktion(void);
int copy_UVR2winsol_1611(u_DS_UVR1611_UVR61_3 *dsatz_uvr1611, DS_Winsol *dsatz_winsol );
int copy_UVR2winsol_1611_CAN(s_DS_CAN *dsatz_uvr1611, DS_Winsol  *dsatz_winsol );
int copy_UVR2winsol_61_3(u_DS_UVR1611_UVR61_3 *dsatz_uvr61_3, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3 );
int copy_UVR2winsol_D1_1611(u_modus_D1 *dsatz_modus_d1, DS_Winsol *dsatz_winsol , int geraet);
int copy_UVR2winsol_D1_61_3(u_modus_D1 *dsatz_modus_d1, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3, int geraet);
int datenlesen_A8(int anz_datensaetze);
int datenlesen_D1(int anz_datensaetze);
int datenlesen_DC(int anz_datensaetze);
int berechneKopfpruefziffer_D1(KopfsatzD1 derKopf[] );
int berechneKopfpruefziffer_A8(KopfsatzA8 derKopf[] );
int berechneKopfpruefziffer_DC(KOPFSATZ_DC derKopf[] );
int berechnepruefziffer_uvr1611(u_DS_UVR1611_UVR61_3 ds_uvr1611[]);
int berechnepruefziffer_uvr61_3(u_DS_UVR1611_UVR61_3 ds_uvr61_3[]);
int berechnepruefziffer_uvr1611_CAN(u_DS_CAN ds_uvr1611[], int anzahl_can_rahmen);
int berechnepruefziffer_modus_D1(u_modus_D1 ds_modus_D1[], int anzahl);
int anzahldatensaetze_D1(KopfsatzD1 kopf[]);
int anzahldatensaetze_A8(KopfsatzA8 kopf[]);
int anzahldatensaetze_DC(KOPFSATZ_DC kopf[]);
int reset_datenpuffer_usb(int do_reset );
int reset_datenpuffer_ip(int do_reset );
void zeitstempel(void);
float berechnetemp(UCHAR lowbyte, UCHAR highbyte);
float berechnevol(UCHAR lowbyte, UCHAR highbyte);
int clrbit( int word, int bit );
int tstbit( int word, int bit );
int xorbit( int word, int bit );
int setbit( int word, int bit );
int ip_handling(int sock);

int csv = 0;    /* Speichern im csv-Format (csv = 1) oder winsol-Format (csv = 0) */
int reset = 0;    /* Ruecksetzen des DL nach Lesen der Daten 1=>true, 0=>false */
int fd_serialport,  write_erg; /* anz_datensaetze; */
struct sockaddr_in SERVER_sockaddr_in;

int csv_header_done=-1;

FILE *fp_logfile=NULL, *fp_logfile_2=NULL,  *fp_logfile_3=NULL, *fp_logfile_4=NULL,
     *fp_logfile_5=NULL, *fp_logfile_6=NULL,  *fp_logfile_7=NULL, *fp_logfile_8=NULL,
     *fp_varlogfile=NULL, *fp_csvfile=NULL ; /* pointer IMMER initialisieren und vor benutzung pruefen */

char dlport[13]; /* Uebergebener Parameter USB-Port */
char LogFileName[255], LogFileName_1[255], LogFileName_2[255], LogFileName_3[255],
	 LogFileName_4[255], LogFileName_5[255], LogFileName_6[255], LogFileName_7[255], 
	 LogFileName_8[255];
char varLogFile[22];
char DirName[241];
char sDatum[11], sZeit[11];
u_DS_UVR1611_UVR61_3 structDLdatensatz;

struct termios oldtio; /* will be used to save old port settings */

int ip_zugriff, ip_first;
int usb_zugriff;
UCHAR uvr_modus, uvr_typ, uvr_typ2;  /* uvr_typ2 -> 2. Geraet bei 2DL */
UCHAR *start_adresse=NULL, *end_adresse=NULL;
KopfsatzD1 kopf_D1[1];
KopfsatzA8 kopf_A8[1];
KOPFSATZ_DC kopf_DC[1];
int sock;


int main(int argc, char *argv[])
{
  struct termios newtio; /* will be used for new port settings */

  int i, anz_ds=0, erg_check_arg, i_varLogFile, erg=0;
  char *pvarLogFile;

  ip_zugriff = 0;
  usb_zugriff = 0;
  ip_first = 1;
  
  strcpy(DirName,"./");
  erg_check_arg = check_arg_getopt(argc, argv);

  printf("    Version 0.9.0 -CAN_Test- vom 14.07.2010 \n");
  
#if  DEBUG>1
  fprintf(stderr, "Ergebnis vom Argumente-Check %d\n",erg_check_arg);
  fprintf(stderr, "angegebener Port: %s Variablen: reset = %d csv = %d \n",dlport,reset,csv);
#endif

  if ( erg_check_arg != 1 )
    exit(-1);

  /* LogFile zum Speichern von Ausgaben aus diesem Programm hier */
  pvarLogFile = &varLogFile[0];
  sprintf(pvarLogFile,"./dl-lesenx.log");

  if ((fp_varlogfile=fopen(varLogFile,"a")) == NULL)
    {
      printf("Log-Datei %s konnte nicht erstellt werden\n",varLogFile);
      i_varLogFile = -1;
      fp_varlogfile=NULL;
    }
  else
    i_varLogFile = 1;


  if ( csv == 1 && (fp_csvfile=fopen("alldata.csv","a")) == NULL )
  {
    printf("csv file %s konnte nicht erstellt werden\n",varLogFile);

    int want_to_continue_even_if_csv_file_failed=1;
    if (want_to_continue_even_if_csv_file_failed)
      printf("  trotzdem wird versucht ein logfile zu schreiben\n!");
    else
    {
      do_cleanup();
      return -1;
    }
  }

  /* IP-Zugriff  - IP-Adresse und Port sind manuell gesetzt!!! */
  if (ip_zugriff && !usb_zugriff)
  {
    /* PF_INET instead of AF_INET - because of Protocol-family instead of address family !? */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
      perror("socket failed()");
      do_cleanup();
      return 2;
    }

    if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
    {
      perror("connect failed()");
      do_cleanup();
      return 3;
    }

    if (ip_handling(sock) == -1)
    {
      fprintf(stderr, "%s: Fehler im Initialisieren der IP-Kommunikation\n", argv[0]);
      do_cleanup();
      return 4;
    }
    //  close(sock); /* IP-Socket schliessen */

  } /* Ende IP-Zugriff */
  else  if (usb_zugriff && !ip_zugriff)
  {
    /* first open the serial port for reading and writing */
    fd_serialport = open(dlport, O_RDWR | O_NOCTTY );
    if (fd_serialport < 0)
    {
      zeitstempel();
      if (fp_varlogfile)
        fprintf(fp_varlogfile,"%s - %s -- kann nicht auf port %s zugreifen \n",sDatum, sZeit,dlport);
        perror(dlport);
        do_cleanup();
        return (-1);
    }

    /* save current port settings */
    tcgetattr(fd_serialport,&oldtio);
    /* initialize the port settings structure to all zeros */
    //bzero( &newtio, sizeof(newtio));
    memset( &newtio, 0, sizeof(newtio) );
    /* then set the baud rate, handshaking and a few other settings */
    newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until first char received */

    tcflush(fd_serialport, TCIFLUSH);
    tcsetattr(fd_serialport,TCSANOW,&newtio);
  }
  else
  {
    fprintf(stderr, " da lief was falsch ....\n %s \n", argv[0]);
    do_cleanup();
  }

  uvr_modus = get_modulmodus(); /* Welcher Modus 
                                0xA8 (1DL) / 0xD1 (2DL) / 0xDC (CAN) */
								
  if ( uvr_modus == 0xDC )
  {
    fprintf(stderr, " CAN-Logging erkannt.\n");
  }

  /* ************************************************************************   */
  /* Lesen des Kopfsatzes zur Ermittlung der Anzahl der zu lesenden Datensaetze */
  i=1;
  do                        /* max. 5 durchlgaenge */
  {
#ifdef DEBUG
      fprintf(stderr, "\n Kopfsatzlesen - Versuch%d\n",i);
#endif
      anz_ds = kopfsatzlesen();
      i++;
  }
  while((anz_ds == -1) && (i < 6));
  
  fprintf(stderr, " CAN-Logging-Test: Kopfsatzlesen fertig, Anzahl Datensaetze: %d\n",anz_ds); /********************/
  
  switch(anz_ds)
  {
    case -1: printf(" Kopfsatzlesen fehlgeschlagen\n");
             do_cleanup();
             return ( -1 );
    case -2: printf(" Keine Daten vorhanden\n");
             do_cleanup();
             return ( -1 );
    case -3: printf(" CAN-Logging: BL-Net nicht bereit!\n");
             do_cleanup();
             return ( -1 );
  }

  printf(" Kopfsatzlesen erfolgreich!\n\n");
  if (uvr_typ == UVR1611)
    printf(" UVR Typ: UVR1611\n");
  if (uvr_typ == UVR61_3)
    printf(" UVR Typ: UVR61-3\n");
	
  zeitstempel();
  fprintf(fp_varlogfile,"%s - %s -- %d Datensaetze im D-LOGG\n\n",sDatum, sZeit,anz_ds);

  /* ************************************************************************ */
  /* Daten aus dem D-LOGG lesen und in das Log-File schreiben                 */
  /* falls csv gesetzt ist auch Daten in allcsv.csv dumpen                    */
  switch(uvr_modus)
  {
    case 0xA8: erg = datenlesen_A8(anz_ds); break;
    case 0xD1: erg = datenlesen_D1(anz_ds); break;
	case 0xDC: erg = datenlesen_DC(anz_ds); break;
  }

  printf("\n%d Datensaetze insgesamt geschrieben.\n",erg);
  zeitstempel();
if (erg != 999) /*  <-- Test CAN, temporaer */
  fprintf(fp_varlogfile,"%s - %s -- Es wurden %d Datensaetze geschrieben.\n",sDatum, sZeit,erg);

  /* ************************************************************************ */
  /* Datenspeicher im D-LOGG zuruecksetzen falls res flag gesetzt */
  if (usb_zugriff && !ip_zugriff)
    reset_datenpuffer_usb( reset );
  else if (ip_zugriff && !usb_zugriff)
    reset_datenpuffer_ip( reset );
  /* ************************************************************************ */

  /* restore the old port settings before quitting */
  //tcsetattr(fd_serialport,TCSANOW,&oldtio);

  int retval=0;
  if ( close_logfile() == -1)
  {
    printf("Cannot close logfile!");
    retval=-1;
  }

  if ( do_cleanup() < 0 )
    retval=-1;

  return (retval);
}

/* Aufraeumen und alles Schliessen */
int do_cleanup(void)
{
  int retval=0;

  if ( fd_serialport > 0 )
    {
      // reset and close
      tcflush(fd_serialport, TCIFLUSH);
      tcsetattr(fd_serialport,TCSANOW,&oldtio);
    }

  if (ip_zugriff)
    close(sock); /* IP-Socket schliessen */

  if (csv==1 && fp_csvfile )
    {
      if ( fclose(fp_csvfile) != 0 )
  {
    printf("Cannot close csvfile %s!",varLogFile);
    retval=-1;
  }
      else
  fp_csvfile=NULL;
    }

  if (fp_varlogfile)
    {
      if ( fclose(fp_varlogfile) != 0 )
  {
    printf("Cannot close %s!",varLogFile);
    retval=-1;
  }
      else
  fp_varlogfile=NULL;
    }
  return retval;
}

/* Hilfetext */
static int print_usage()
{

  fprintf(stderr,"\ndl-lesenx (-p USB-Port | -i IP:Port)  [--csv] [--res] [--dir] [-h] [-v]\n");
  fprintf(stderr,"    -p USB-Port -> Angabe des USB-Portes,\n");
  fprintf(stderr,"                   an dem der D-LOGG angeschlossen ist.\n");
  fprintf(stderr,"    -i IP:Port  -> Angabe der IP-Adresse / Hostname und des Ports,\n");
  fprintf(stderr,"                   an dem der BL-Net angeschlossen ist.\n");
  fprintf(stderr,"          --csv -> im CSV-Format speichern (wird noch nicht unterstuetzt)\n");
  fprintf(stderr,"                   Standard: ist WINSOL-Format\n");
  fprintf(stderr,"          --res -> nach dem Lesen Ruecksetzen des DL\n");
  fprintf(stderr,"                   Standard: KEIN Ruecksetzen des DL nach dem Lesen\n");
  fprintf(stderr,"          --dir -> Verzeichnis in dem die Datei angelegt werden soll\n");
  fprintf(stderr,"          -h    -> diesen Hilfetext\n");
  fprintf(stderr,"          -v    -> Versionsangabe\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"Beispiel: dl-lesenx -p /dev/ttyUSB0 --res\n");
  fprintf(stderr,"          Liest die Daten vom USB-Port 0 und setzt den D-LOGG zurueck.\n");
  fprintf(stderr,"          dl-lesenx -i blnetz:40000 --res\n");
  fprintf(stderr,"          Liest die Daten vom Host blnetz und setzt den D-LOGG zurueck.\n");
  fprintf(stderr,"          dl-lesenx -i 192.168.0.10:40000 \n");
  fprintf(stderr,"          Liest die Daten von der IP-Adresse 192.168.0.10 und setzt den D-LOGG nicht zurueck.\n");
  fprintf(stderr,"\n");
  return 0;

}

/* Auswertung der uebergebenen Argumente */
int check_arg_getopt(int arg_c, char *arg_v[])
{

  int c = 0;
  int p_is_set=-1;
  int i_is_set=-1;
  char trennzeichen[] = ":";

  /* arbeitet alle argumente ab  */
  while (1)
  {
    int option_index = 0;
    static struct option long_options[] =
    {
      {"csv", 0, 0, 0},
      {"res", 0, 0, 0},
      {"dir", 1, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long(arg_c, arg_v, "hvp:i:", long_options, &option_index);
    if (c == -1)  /* ende des Parameter-processing */
    {
      if (optind <2)
      {
        print_usage(); /* -p is a non-optional argument */
        return -1;
      }
      break;
    }

    switch(c)
    {
      case 'v':
      {
        printf("\n    UVR1611/UVR61-3 Daten lesen vom D-LOGG USB / BL-Net \n");
        printf("    Version 0.9.0 -CAN_Test- vom 14.07.2010 \n");
        return 0;
      }
      case 'h':
      case '?':
        print_usage();
        return -1;
      case 'p':
      {
        if ( strlen(optarg) < 13)
        {
          strncpy(dlport, optarg , strlen(optarg));
          printf("\n port gesetzt: %s\n", dlport);
          p_is_set=1;
          usb_zugriff = 1;
        }
        else
        {
          printf(" portname zu lang: %s\n",optarg);
          print_usage();
          return -1;
        }
        break;
      }
      case 'i':
      {
        struct hostent* hostinfo = gethostbyname(strtok(optarg,trennzeichen));
        if(0 == hostinfo)
        {
          fprintf(stderr," IP-Adresse konnte nicht aufgeloest werden: %s\n",optarg);
          print_usage();
          return -1;
        } 
        else 
        {
          SERVER_sockaddr_in.sin_addr = *(struct in_addr*)*hostinfo->h_addr_list;
          SERVER_sockaddr_in.sin_port = htons((unsigned short int) atol(strtok(NULL,trennzeichen)));
          SERVER_sockaddr_in.sin_family = AF_INET;
          fprintf(stderr,"\n Adresse:port gesetzt: %s:%d\n", inet_ntoa(SERVER_sockaddr_in.sin_addr),
          ntohs(SERVER_sockaddr_in.sin_port));
          i_is_set=1;
          ip_zugriff = 1;
	}
        //~ if ( strlen(optarg) < 22)
        //~ {
          //~ SERVER_sockaddr_in.sin_addr.s_addr = inet_addr(strtok(optarg,trennzeichen));
          //~ SERVER_sockaddr_in.sin_port = htons((unsigned short int) atol(strtok(NULL,trennzeichen)));
          //~ SERVER_sockaddr_in.sin_family = AF_INET;
          //~ fprintf(stderr,"\n Adresse:port gesetzt: %s:%d\n", inet_ntoa(SERVER_sockaddr_in.sin_addr),
          //~ ntohs(SERVER_sockaddr_in.sin_port));
          //~ i_is_set=1;
          //~ ip_zugriff = 1;
        //~ }
        //~ else
        //~ {
          //~ fprintf(stderr," IP-Adresse zu lang: %s\n",optarg);
          //~ print_usage();
          //~ return -1;
        //~ }
        break;
      }
      case 0:
      {
        if (  strncmp( long_options[option_index].name, "csv", 3) == 0 ) /* csv-Format ist gewuenscht */
        {
          csv = 0;
          printf(" Zur Zeit keine csv-Ausgabe!\n");
      //    csv = 1;
     //     printf(" zusaetzlich Ausgabe in csv file\n");
     //  01.2008 - vorerst wieder deaktiviert, muss ueberarbeitet werden 
        }
        if (  strncmp( long_options[option_index].name, "res", 3) == 0 )
        {
          reset = 1;  /* Ruecksetzen DL nach Daten lesen gewuenscht */
          printf(" mit  reset des Logger-Speichers! \n");
        }
        if (  strncmp( long_options[option_index].name, "dir", 3) == 0 ) /* Verzeichnis */
        {
          if (strlen(optarg) < (sizeof(DirName)-1)) {
            strcpy(DirName,optarg);
            if (DirName[strlen(DirName)] != '/') {
              strcat(DirName,"/");
            }
          }
          else {
          printf(" Verzeichnis ist zu lang\n");
          }
        }
        break;
      }
      default:
        printf ("?? input mit character code 0%o ??\n", c);
        printf("Falsche Parameterangebe!\n");
        print_usage();
        return( -1 );
    }
  }
  /* Restliche non-option arguments */

  if (( p_is_set < 1) && ( i_is_set < 1 ) )
    {
      fprintf(stderr," USB-Port- oder IP-Angabe fehlt!\n");
      print_usage();
      return -1;
    }
  if (( p_is_set  > 0) && ( i_is_set > 0 ) )
    {
      fprintf(stderr," Auslesen nicht gleichzeitig von USB-Port- und IP-Port moeglich!\n");
      print_usage();
      return -1;
    }

  /* falls du noch andere argumente prozessieren willst bzw. junk hinten dran.... */

  if (optind < arg_c)
    {
      printf ("non-option ARGV-elements: ");
      while (optind < arg_c)
  printf ("%s ", arg_v[optind++]);
      printf ("\n");
      return -1;
    }

  return 1;
}


/* Der Name des Logfiles wird aus dem eigelesenen Wert fuer Jahr und Monat erzeugt */
int erzeugeLogfileName(UCHAR ds_monat, UCHAR ds_jahr)
{
  int erg = 0;
  char csv_endung[] = ".csv", winsol_endung[] = ".log";
  char *pLogFileName=NULL, *pLogFileName_2=NULL;
  pLogFileName = LogFileName;
  pLogFileName_2 = LogFileName_2;

  if (csv ==  1) /* LogDatei im CSV-Format schreiben */
    {
      erg=sprintf(pLogFileName,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung);
      if (uvr_modus == 0xD1)
        erg=sprintf(pLogFileName_2,"%s2%03d%02d_2%s",DirName,ds_jahr,ds_monat,csv_endung);
    }
  else  /* LogDatei im Winsol-Format schreiben */
    {
      erg=sprintf(pLogFileName,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung);
      if (uvr_modus == 0xD1)
        erg=sprintf(pLogFileName_2,"%sY2%03d%02d_2%s",DirName,ds_jahr,ds_monat,winsol_endung);
    }

  return erg;
}

/* Der Name der Logfiles wird aus dem eigelesenen Wert fuer Jahr und Monat erzeugt */
int erzeugeLogfileName_CAN(UCHAR ds_monat, UCHAR ds_jahr, int anzahl_Rahmen)
{
  int erg = 0;
  char csv_endung[] = ".csv", winsol_endung[] = ".log";
  char *pLogFileName_1=NULL, *pLogFileName_2=NULL, *pLogFileName_3=NULL, *pLogFileName_4=NULL, *pLogFileName_5=NULL,
  *pLogFileName_6=NULL, *pLogFileName_7=NULL, *pLogFileName_8=NULL;
  
  pLogFileName_1 = LogFileName_1;
  pLogFileName_2 = LogFileName_2;
  pLogFileName_3 = LogFileName_3;
  pLogFileName_4 = LogFileName_4;
  pLogFileName_5 = LogFileName_5;
  pLogFileName_6 = LogFileName_6;
  pLogFileName_7 = LogFileName_7;
  pLogFileName_8 = LogFileName_8;

  if (csv ==  1) /* LogDatei im CSV-Format schreiben */
    {
	  switch(anzahl_Rahmen) 
	  {
	    case 1: erg=sprintf(pLogFileName_1,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung); break;
		
		case 2: erg=sprintf(pLogFileName_1,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_2,"%s2%03d%02d_2%s",DirName,ds_jahr,ds_monat,csv_endung); break;
				
		case 3: erg=sprintf(pLogFileName_1,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_2,"%s2%03d%02d_2%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_3,"%s2%03d%02d_3%s",DirName,ds_jahr,ds_monat,csv_endung); break;
				
		case 4: erg=sprintf(pLogFileName_1,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_2,"%s2%03d%02d_2%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_3,"%s2%03d%02d_3%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_4,"%s2%03d%02d_4%s",DirName,ds_jahr,ds_monat,csv_endung); break;
				
		case 5: erg=sprintf(pLogFileName_1,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_2,"%s2%03d%02d_2%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_3,"%s2%03d%02d_3%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_4,"%s2%03d%02d_4%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_5,"%s2%03d%02d_5%s",DirName,ds_jahr,ds_monat,csv_endung); break;
				
		case 6: erg=sprintf(pLogFileName_1,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_2,"%s2%03d%02d_2%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_3,"%s2%03d%02d_3%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_4,"%s2%03d%02d_4%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_5,"%s2%03d%02d_5%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_6,"%s2%03d%02d_6%s",DirName,ds_jahr,ds_monat,csv_endung); break;
				
		case 7: erg=sprintf(pLogFileName_1,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_2,"%s2%03d%02d_2%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_3,"%s2%03d%02d_3%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_4,"%s2%03d%02d_4%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_5,"%s2%03d%02d_5%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_6,"%s2%03d%02d_6%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_7,"%s2%03d%02d_7%s",DirName,ds_jahr,ds_monat,csv_endung); break;
				
		case 8: erg=sprintf(pLogFileName_1,"%s2%03d%02d%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_2,"%s2%03d%02d_2%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_3,"%s2%03d%02d_3%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_4,"%s2%03d%02d_4%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_5,"%s2%03d%02d_5%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_6,"%s2%03d%02d_6%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_7,"%s2%03d%02d_7%s",DirName,ds_jahr,ds_monat,csv_endung);
		        erg=sprintf(pLogFileName_8,"%s2%03d%02d_8%s",DirName,ds_jahr,ds_monat,csv_endung); break;
	  }
    }
  else  /* LogDatei im Winsol-Format schreiben */
    {
	  switch(anzahl_Rahmen) 
	  {
	    case 1: erg=sprintf(pLogFileName_1,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung); break;
		
		case 2: erg=sprintf(pLogFileName_1,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_2,"%sY2%03d%02d_2%s",DirName,ds_jahr,ds_monat,winsol_endung); break;
				
		case 3: erg=sprintf(pLogFileName_1,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_2,"%sY2%03d%02d_2%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_3,"%sY2%03d%02d_3%s",DirName,ds_jahr,ds_monat,winsol_endung); break;
				
		case 4: erg=sprintf(pLogFileName_1,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung);
//	fprintf(stderr,"---> LogDateiNamenErzeugung 4 Datenrahmen, 1. Logdatei Ergebnis: %d - %s\n",erg,LogFileName_1);
//	fprintf(stderr,"---> Dateiname: %sY2%03d%02d%s \n",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_2,"%sY2%03d%02d_2%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_3,"%sY2%03d%02d_3%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_4,"%sY2%03d%02d_4%s",DirName,ds_jahr,ds_monat,winsol_endung); break;
				
		case 5: erg=sprintf(pLogFileName_1,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung);
//	fprintf(stderr,"---> LogDateiNamenErzeugung 4 Datenrahmen, 1. Logdatei Ergebnis: %d - %s\n",erg,LogFileName_1);
//	fprintf(stderr,"---> Dateiname: %sY2%03d%02d%s \n",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_2,"%sY2%03d%02d_2%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_3,"%sY2%03d%02d_3%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_4,"%sY2%03d%02d_4%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_5,"%sY2%03d%02d_5%s",DirName,ds_jahr,ds_monat,winsol_endung); break;
				
		case 6: erg=sprintf(pLogFileName_1,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung);
//	fprintf(stderr,"---> LogDateiNamenErzeugung 4 Datenrahmen, 1. Logdatei Ergebnis: %d - %s\n",erg,LogFileName_1);
//	fprintf(stderr,"---> Dateiname: %sY2%03d%02d%s \n",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_2,"%sY2%03d%02d_2%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_3,"%sY2%03d%02d_3%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_4,"%sY2%03d%02d_4%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_5,"%sY2%03d%02d_5%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_6,"%sY2%03d%02d_6%s",DirName,ds_jahr,ds_monat,winsol_endung); break;
				
		case 7: erg=sprintf(pLogFileName_1,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung);
//	fprintf(stderr,"---> LogDateiNamenErzeugung 4 Datenrahmen, 1. Logdatei Ergebnis: %d - %s\n",erg,LogFileName_1);
//	fprintf(stderr,"---> Dateiname: %sY2%03d%02d%s \n",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_2,"%sY2%03d%02d_2%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_3,"%sY2%03d%02d_3%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_4,"%sY2%03d%02d_4%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_5,"%sY2%03d%02d_5%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_6,"%sY2%03d%02d_6%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_7,"%sY2%03d%02d_7%s",DirName,ds_jahr,ds_monat,winsol_endung); break;
				
		case 8: erg=sprintf(pLogFileName_1,"%sY2%03d%02d%s",DirName,ds_jahr,ds_monat,winsol_endung);
//	fprintf(stderr,"---> LogDateiNamenErzeugung 4 Datenrahmen, 1. Logdatei Ergebnis: %d - %s\n",erg,LogFileName_1);
//	fprintf(stderr,"---> Dateiname: %sY2%03d%02d%s \n",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_2,"%sY2%03d%02d_2%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_3,"%sY2%03d%02d_3%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_4,"%sY2%03d%02d_4%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_5,"%sY2%03d%02d_5%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_6,"%sY2%03d%02d_6%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_7,"%sY2%03d%02d_7%s",DirName,ds_jahr,ds_monat,winsol_endung);
		        erg=sprintf(pLogFileName_8,"%sY2%03d%02d_8%s",DirName,ds_jahr,ds_monat,winsol_endung); break;
	  }
    }

  return erg;
}

/* Logdatei oeffnen / erstellen */
int open_logfile(char LogFile[], int geraet)
{
  int i, tmp_erg = 0;
  i=-1;  /* es ist kein Logfile geoeffnet (Wert -1) */
  /* bei neuer Logdatei der erste Datensatz: */
  UCHAR kopf_winsol_1611[59]={0x01, 0x02, 0x01, 0x03, 0xF0, 0x0F, 0x00, 0x07, 0xAA, 0xAA, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xFF, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00};
  UCHAR kopf_winsol_61_3[59]={0x01, 0x02, 0x01, 0x03, 0xF0, 0x0F, 0x00, 0x06, 0xAA, 0xAA, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xFF, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00};

  if ( geraet == 1)
  {
    if ((fp_logfile=fopen(LogFile,"r")) == NULL) /* wenn Logfile noch nicht existiert */
    {
      if ((fp_logfile=fopen(LogFile,"w")) == NULL) /* dann Neuerstellung der Logdatei */
      {
        printf("Log-Datei %s konnte nicht erstellt werden\n",LogFile);
        i = -1;
      }
      else
      {
        i = 0;
        if (csv == 0)
        {
          // Unterscheidung UVR1611 / UVR61-3 !!!!
          if (uvr_typ == UVR1611)
            tmp_erg = fwrite(&kopf_winsol_1611,59,1,fp_logfile);
          if (uvr_typ == UVR61_3)
            tmp_erg = fwrite(&kopf_winsol_61_3,59,1,fp_logfile);
          if ( tmp_erg != 1)
          {
            printf("Kopfsatz konnte nicht geschrieben werden!\n");
            i= -1;
          }
        }
        else
        {
          if (uvr_typ == UVR1611)  /* UVR1611 */
            fprintf(fp_logfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
          if (uvr_typ == UVR61_3)  /* UVR61-3 */
            fprintf(fp_logfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Ausg3 ; Analogausg ; Vol-Strom ; Momentanlstg ; MWh ; kWh \n");
          csv_header_done=1;
        }
      }
    }
    else /* das Logfile existiert schon */
    {
      fclose(fp_logfile);
      if ((fp_logfile=fopen(LogFile,"a")) == NULL) /* schreiben ab Dateiende */
      {
        printf("Log-Datei %s konnte nicht geoeffnet werden\n",LogFile);
        i = -1;
      }
      else
      {
        csv_header_done = 1;
        i = 0;
      }
    }
  }
  else
  {
    if ((fp_logfile_2=fopen(LogFile,"r")) == NULL) /* wenn Logfile noch nicht existiert */
    {
      if ((fp_logfile_2=fopen(LogFile,"w")) == NULL) /* dann Neuerstellung der Logdatei */
      {
        printf("Log-Datei %s konnte nicht erstellt werden\n",LogFile);
        i = -1;
      }
      else
      {
        i = 0;
        if (csv == 0)
        {
          // Unterscheidung UVR1611 / UVR61-3 !!!!
          if (uvr_typ2 == UVR1611)
            tmp_erg = fwrite(&kopf_winsol_1611,59,1,fp_logfile_2);
          if (uvr_typ2 == UVR61_3)
            tmp_erg = fwrite(&kopf_winsol_61_3,59,1,fp_logfile_2);
          if ( tmp_erg != 1)
          {
            printf("Kopfsatz konnte nicht geschrieben werden!\n");
            i= -1;
          }
        }
        else
        {
          if (uvr_typ2 == UVR1611)  /* UVR1611 */
            fprintf(fp_logfile_2," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
          if (uvr_typ2 == UVR61_3)  /* UVR61-3 */
            fprintf(fp_logfile_2," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Ausg3 ; Analogausg ; Vol-Strom ; Momentanlstg ; MWh ; kWh \n");
          csv_header_done=1;
        }
      }
    }
    else /* das Logfile existiert schon */
    {
      fclose(fp_logfile_2);
      if ((fp_logfile_2=fopen(LogFile,"a")) == NULL) /* schreiben ab Dateiende */
      {
        printf("Log-Datei %s konnte nicht geoeffnet werden\n",LogFile);
        i = -1;
      }
      else
      {
        csv_header_done = 1;
        i = 0;
      }
    }
  }

  return(i);
}

/* Logdatei CAN oeffnen / erstellen; int datenrahmen => welcher Datenrahmen wird bearbeitet */
int open_logfile_CAN(char LogFile[], int datenrahmen)
{
  FILE *fp_logfile_tmp=NULL;
  int i, tmp_erg = 0;
  i=-1;  /* es ist kein Logfile geoeffnet (Wert -1) */
  /* bei neuer Logdatei der erste Datensatz: */
  UCHAR kopf_winsol_1611[59]={0x01, 0x02, 0x01, 0x03, 0xF0, 0x0F, 0x00, 0x07, 0xAA, 0xAA, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xFF, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00};

fprintf(stderr,"---> in open_logfile_CAN() LogFileName: %s - Datenrahmen: %d\n",LogFile,datenrahmen);

  if ((fp_logfile_tmp=fopen(LogFile,"r")) == NULL) /* wenn Logfile noch nicht existiert */
  {
    if ((fp_logfile_tmp=fopen(LogFile,"w")) == NULL) /* dann Neuerstellung der Logdatei */
    {
      printf("Log-Datei %s konnte nicht erstellt werden\n",LogFile);
      i = -1;
    }
    else
    {
      i = 0;
      if (csv == 0)
      {
        tmp_erg = fwrite(&kopf_winsol_1611,59,1,fp_logfile_tmp);
        if ( tmp_erg != 1)
        {
          printf("Kopfsatz konnte nicht geschrieben werden!\n");
          i= -1;
        }
      }
      else
      {
        fprintf(fp_logfile_tmp," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
        csv_header_done=1;
      }
    }
  }
  else /* das Logfile existiert schon */
  {
  fprintf(stderr,"--> open_logfile_CAN() Logfile existiert, LogFileName: %s - Datenrahmen: %d\n",LogFile,datenrahmen);
    fclose(fp_logfile_tmp);
    if ((fp_logfile_tmp=fopen(LogFile,"a")) == NULL) /* schreiben ab Dateiende */
    {
      printf("Log-Datei %s konnte nicht geoeffnet werden\n",LogFile);
      i = -1;
    }
    else
    {
      csv_header_done = 1;
      i = 0;
    }
  }
  
  switch( datenrahmen )
  {
    case 1: fp_logfile = fp_logfile_tmp; break;
    case 2: fp_logfile_2 = fp_logfile_tmp; break;
    case 3: fp_logfile_3 = fp_logfile_tmp; break;
    case 4: fp_logfile_4 = fp_logfile_tmp; break;
    case 5: fp_logfile_5 = fp_logfile_tmp; break;
    case 6: fp_logfile_6 = fp_logfile_tmp; break;
    case 7: fp_logfile_7 = fp_logfile_tmp; break;
    case 8: fp_logfile_8 = fp_logfile_tmp; break;
  }

  return(i);
}

/* Logdatei schliessen */
int close_logfile(void)
{
  int i = -1;
  
  if (fp_logfile == NULL)
  {
	fprintf(stderr, " Kein Logfile offen.\n");
	return(i);
  }

  i=fclose(fp_logfile);
  if (uvr_modus == 0xD1)
  {
	if (fp_logfile_2 == NULL)
    {
	  fprintf(stderr, " Kein Logfile offen.\n");
	  return(i);
    }
	else
      i=fclose(fp_logfile_2);
  }
  return(i);
}

/*  */
void writeWINSOLlogfile2CSV(FILE * fp_WSLOGcsvfile, const DS_Winsol  *dsatz_winsol, int Jahr ,int Monat )
{
#if 0
  /*  Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 */
  /* ;  ; TSp.Unten ; TSp.Mitte ; TSp,Oben ; TBoilerUnten ; TBoilerOben ; Kollektor ; SolarVL ; SolarRL ; TAussen ; TVerteilVL ; TKessel ; TVerteilVL1 ; THeizkrVL ; THeizkrRL ; TZirkuRL ; TKesselVL1 ; Solarpumpe ;  ; Ladepumpe ;  ; ZirkuPumpe ;  ; BrennerAnf ; HeizkrPumpe ;  ;  ;  ; MischerKalt ; MischerWarm ;  ;  ;  ;  ; SolWMZ ;  ;  ;  ; */
  /* 01.01.07;00:00:30;  29,4;  29,7;  45,8;  56,9;  58,2;   8,9;  19,3;  19,0;   6,5;  36,7;  46,7;  26,2;  26,7;  25,9;  28,2;  53,2;
 0; 0; 0; --; 0; 0; 0; 1; --; 0; --; 0; 0; 0; 0; 0; 0;  0,00;  52,0;  ----;  ----; */
  /* 01.01.07;00:01:30;  29,4;  29,7;  45,8;  56,9;  58,1;   9,0;  19,3;  19,0;   6,5;  36,7;  46,6;  26,1;  26,6;  26,0;  28,2;  53,2; 0; 0; 0; --; 0; 0; 0; 1; --; 0; --; 0; 0; 0; 0; 0; 0;  0,00;  52,0;  ----;  ----; */
#endif

  int ii, z, zwi_wmzaehler1, zwi_wmzaehler2;
  int ausgaenge[13];
  float momentLstg1, kwh1, mwh1, momentLstg2, kwh2, mwh2;

  if ( csv_header_done < 0 )
  {
    if (uvr_typ == UVR1611)  /* UVR1611 */
      fprintf(fp_WSLOGcsvfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
//    fprintf(fp_WSLOGcsvfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 ");
//    fprintf(fp_WSLOGcsvfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 ");

    if (uvr_typ == UVR61_3)  /* UVR61-3 */
      fprintf(fp_WSLOGcsvfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Ausg3 ; Analogausg ; Vol-Strom ; Momentanlstg ; MWh ; kWh \n");
      csv_header_done=1;
  }

  fprintf(fp_WSLOGcsvfile,"%02d.%02d.%02d;%02d:%02d:%02d;", dsatz_winsol[0].tag,Monat,Jahr, dsatz_winsol[0].std,dsatz_winsol[0].min,dsatz_winsol[0].sek);

  /* need to test for: */
  /* used/unused sensor */
  /* type of sensor */
//#ifdef NEW_WSOL_TEMP
  for (ii=0;ii<16;ii++)
    fprintf(fp_WSLOGcsvfile,"%.1f;", berechnetemp(dsatz_winsol[0].sensT[ii][0],dsatz_winsol[0].sensT[ii][1]) );

  /* Ausgaenge */
  /* 1. Byte AAAA AAAA */
  /* 2. Byte xxxA AAAA  */
  /* x  ... dont care   */
  /* A ... Ausgang (von rechts nach links zu nummerieren) */

  for(z=0;z<8;z++)
  {
    if (tstbit( dsatz_winsol[0].ausgbyte1, 7-z ) == 1)
    {
      ausgaenge[z] = 1;
//      printf (" A%d ja\n",z+1);
    }
    else
    {
      ausgaenge[z] = 0;
//      printf (" A%d nein\n",z+1);
    }
  }

  for(z=0;z<5;z++)
  {
    if ( tstbit( dsatz_winsol[0].ausgbyte2, 7-z ) == 1)
    {
//      printf (" A%d  1 \n",z+8);
      ausgaenge[z+7] = 1;
    }
    else
    {
//      printf (" A%d 0\n",z+8);
      ausgaenge[z+7] = 0;
    }
  }

  /* 4 Drehzahlstufe je 1byte */
  /* Bitbelegung      NxxD DDDD  */
  /* N ...Drehzahlregelung aktiv (0) */
  /* D ... Drehzahlstufe (0-30) */
  /* x  ... dont care  */

  for(z=0;z<13;z++)
  {
//    if ( ausgaenge[z] )
//      printf("A%i  Zustand: 1               ",z+1);
//    else
//      printf("A%i  Zustand: 0               ",z+1);

    if (z==0 || z==1 || z==5 || z==6 )
    {
      int index=z;
      if (z>4) index-=3;
//        printf("Drehzahlregler DA%d Zustand: %i     ", z+1,tstbit( dsatz_winsol[0].dza[z], 7 ));

      UCHAR temp_highbyte;
      temp_highbyte = dsatz_winsol[0].dza[index];

#ifdef DEBUG
         fprintf(stderr, " Drehz.-Stufe highbyte: %X  \n", temp_highbyte);
#endif
      int jj=0;
      for(jj=5;jj<9;jj++)
        temp_highbyte = clrbit(temp_highbyte,jj); /* die oberen EEE Bits auf 0 setzen */

//         printf ("  Drehzst_A%d: %d\n",z+1,(int)temp_highbyte);

      if ( tstbit( dsatz_winsol[0].dza[z], 7 ) )
      {
        fprintf(fp_WSLOGcsvfile," %d; %d;", ausgaenge[z],(int)temp_highbyte); /*  DRehz Ausgang mit Drehzahl */
      }
      else
        fprintf(fp_WSLOGcsvfile," %d; --;", ausgaenge[z]); /* Drehz Ausgang als digitaler Ausgang */
    }
    else
      fprintf(fp_WSLOGcsvfile," %d; --;", ausgaenge[z]); /* kein Drehz Ausgang */
  }


  /*    printf("Byte Waermemengenzaehler: %x\n",akt_daten[39]); */
  switch(dsatz_winsol[0].wmzaehler_reg )
  {
    case 1:
      zwi_wmzaehler1 = 1; break; /* Waermemengenzaehler1 */
    case 2:
      zwi_wmzaehler2 = 1; break; /* Waermemengenzaehler2 */
    case 3:
      zwi_wmzaehler1 = 1;        /* Waermemengenzaehler1 */
      zwi_wmzaehler2 = 1;   /* Waermemengenzaehler2 */
      break;
  }

  if (zwi_wmzaehler1 == 1)
  {
    momentLstg1 = (10*(65536*(float)dsatz_winsol[0].mlstg1[3]
         + 256*(float)dsatz_winsol[0].mlstg1[2]
         +(float)dsatz_winsol[0].mlstg1[1])
         +((float)dsatz_winsol[0].mlstg1[0]*10)/256)/100;
//      printf("Momentanleistung 1: %.1f\n",momentLstg1);
    kwh1 = ( (float)dsatz_winsol[0].kwh1[1] *256 + (float)dsatz_winsol[0].kwh1[0] ) / 10;
    mwh1 = (float)dsatz_winsol[0].mwh1[1] *0x100 + (float)dsatz_winsol[0].mwh1[0];
//      printf("Waermemengenzaehler 1: %.0f MWh und %.1f kWh\n",mwh1,kwh1);

    fprintf(fp_WSLOGcsvfile," %.1f; %.1f;",momentLstg1,kwh1);
  }
  else
    fprintf(fp_WSLOGcsvfile," --; --;");

  if (zwi_wmzaehler2 == 1)
  {
    momentLstg2 = (10*(65536*(float)dsatz_winsol[0].mlstg2[3]
       + 256*(float)dsatz_winsol[0].mlstg2[2]
       +(float)dsatz_winsol[0].mlstg2[1])
       +((float)dsatz_winsol[0].mlstg2[0]*10)/256)/100;
//      printf("Momentanleistung 1: %.1f\n",momentLstg2);
    kwh2 = ( (float)dsatz_winsol[0].kwh2[1] *256 + (float)dsatz_winsol[0].kwh2[0] ) / 10;
    mwh2 = (float)dsatz_winsol[0].mwh2[1] *0x100 + (float)dsatz_winsol[0].mwh2[0];
//      printf("Waermemengenzaehler 1: %.0f MWh und %.1f kWh\n",mwh2,kwh2);

    fprintf(fp_WSLOGcsvfile," %.1f; %.1f;",momentLstg1,kwh1);
  }
  else
    fprintf(fp_WSLOGcsvfile," --; --;");

  fprintf(fp_WSLOGcsvfile,"\n");
}

/* Modulmoduskennung abfragen */
int get_modulmodus(void)
{
  int result;
  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  int empfbuf[1];

  sendbuf[0]=VERSIONSABFRAGE;    /* Senden der Kopfsatz-abfrage */

/* ab hier unterscheiden nach USB und IP */
  if (usb_zugriff)
  {
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd_serialport,empfbuf,1);
  }
  if (ip_zugriff)
  {
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
     }  /* if (!ip_first) */
    write_erg=send(sock,sendbuf,1,0);
     if ( write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
  }

  return empfbuf[0];
}


/* Kopfsatz vom DL lesen und Rueckgabe Anzahl Datensaetze */
int kopfsatzlesen(void)
{
  int result, anz_ds, pruefz, merk_pruefz, durchlauf;
  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  durchlauf=0;

  do
  {
      sendbuf[0]=KOPFSATZLESEN;    /* Senden der Kopfsatz-abfrage */

    if (usb_zugriff)
    {
      write_erg=write(fd_serialport,sendbuf,1);
      if (write_erg == 1)    /* Lesen der Antwort*/
      {
	    switch(uvr_modus)
		{
          case 0xD1: result=read(fd_serialport,kopf_D1,14); break;
          case 0xA8: result=read(fd_serialport,kopf_A8,13); break;
		  case 0xDC: result=read(fd_serialport,kopf_DC,21); 
		            if (kopf_DC[0].all_bytes[0] == 0xAA)
					{
						fprintf(stderr, " CAN-Logging: BL-Net noch nicht bereit, 3 Sekunden warten...\n");
						sleep(3000); /* 3 Sekunden warten */
						write_erg=write(fd_serialport,sendbuf,1);
						if (write_erg == 1)    /* Lesen der Antwort*/
						{
							if (kopf_DC[0].all_bytes[0] == 0xAA)
							{
								fprintf(stderr, " CAN-Logging: BL-Net immer noch nicht bereit. Abbruch!\n");
								return ( -3 );
							}
							else if (kopf_DC[0].all_bytes[0] != 0xAA)
								result=read(fd_serialport,kopf_DC,21);
						}
					}
					break;
        }
      }
    }

    if (ip_zugriff)
    {
      if (!ip_first)
      {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup();
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup();
          return 3;
        }
      }  /* if (!ip_first) */
      write_erg=send(sock,sendbuf,1,0);
      if (write_erg == 1)    /* Lesen der Antwort */
      {
	    switch(uvr_modus)
		{
          case 0xD1: result = recv(sock,kopf_D1,14,0); break;
          case 0xA8: result = recv(sock,kopf_A8,13,0); break;
		  case 0xDC: result = recv(sock,kopf_DC,21,0);
		            if (kopf_DC[0].all_bytes[0] == 0xAA)
					{
						fprintf(stderr, " CAN-Logging: BL-Net noch nicht bereit, 3 Sekunden warten...\n");
						sleep(3000); /* 3 Sekunden warten */
						write_erg=write(fd_serialport,sendbuf,1);
						if (write_erg == 1)    /* Lesen der Antwort*/
						{
							if (kopf_DC[0].all_bytes[0] == 0xAA)
							{
								fprintf(stderr, " CAN-Logging: BL-Net immer noch nicht bereit. Abbruch!\n");
								return ( -3 );
							}
							else if (kopf_DC[0].all_bytes[0] != 0xAA)
								result=read(fd_serialport,kopf_DC,21);
						}
					}
					break;
        }
      }
    }

    switch(uvr_modus)
    {
      case 0xD1: 
	    pruefz = berechneKopfpruefziffer_D1( kopf_D1 );
	    merk_pruefz = kopf_D1[0].pruefsum;
        break;
      case 0xA8: 
        pruefz = berechneKopfpruefziffer_A8( kopf_A8 );
        merk_pruefz = kopf_A8[0].pruefsum;
		break;
	  case 0xDC: 
//		fprintf(stderr, " CAN-Logging-Test: Anzahl Datenrahmen laut Byte 6: %x\n",kopf_DC[0].all_bytes[5]); 
	    pruefz = berechneKopfpruefziffer_DC( kopf_DC );
		switch(kopf_DC[0].all_bytes[5])
		{
		  case 1: merk_pruefz = kopf_DC[0].DC_Rahmen1.pruefsum; 
//		        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen1.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen1.pruefsum);
                break;
		  case 2: merk_pruefz = kopf_DC[0].DC_Rahmen2.pruefsum; 
//		        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen2.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen2.pruefsum);
		        break;
		  case 3: merk_pruefz = kopf_DC[0].DC_Rahmen3.pruefsum; 
//		        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen3.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen3.pruefsum);
		        break;
		  case 4: merk_pruefz = kopf_DC[0].DC_Rahmen4.pruefsum; 
//		        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen4.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen4.pruefsum);
		        break;
		  case 5: merk_pruefz = kopf_DC[0].DC_Rahmen5.pruefsum; 
//		        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen5.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen5.pruefsum);
		        break;
		  case 6: merk_pruefz = kopf_DC[0].DC_Rahmen6.pruefsum; 
//		        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen6.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen6.pruefsum);
		        break;
		  case 7: merk_pruefz = kopf_DC[0].DC_Rahmen7.pruefsum; 
//		        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen7.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen7.pruefsum);
		        break;
		  case 8: merk_pruefz = kopf_DC[0].DC_Rahmen8.pruefsum; 
//		        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen8.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen8.pruefsum);
		        break;
		  default: 
				fprintf(stderr,"  CAN-Logging-Test:  Kennung %x\n",kopf_DC[0].all_bytes[0]);
		}
        break;
	}

    durchlauf++;
   #ifdef DEBUG
    if ( uvr_modus == 0xD1 )
      fprintf(stderr, "  Durchlauf #%d  berechnete pruefziffer:%d kopfsatz.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_D1[0].pruefsum);
    if ( uvr_modus == 0xA8 )
      fprintf(stderr, "  Durchlauf #%d  berechnete pruefziffer:%d kopfsatz.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_A8[0].pruefsum);
    if ( uvr_modus == 0xDC )
      fprintf(stderr, "  Durchlauf #%d  berechnete pruefziffer:%d kopfsatz.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].pruefsum);
   #endif
  }
  while (( (pruefz != merk_pruefz )  && (durchlauf < 10)));

#ifdef DEBUG
  if (pruefz != merk_pruefz )
    {
      fprintf(stderr, " Durchlauf #%i - falsche Pruefziffer im Kopfsatz\n",durchlauf);
      return -1;
    }
  else
    fprintf(stderr, "Anzahl Durchlaeufe Pruefziffer Kopfsatz: %i\n",durchlauf);
#endif

  /* Startadresse der Daten */
  switch(uvr_modus)
  {
    case 0xD1: start_adresse = kopf_D1[0].startadresse;
			   end_adresse = kopf_D1[0].endadresse;
      break;
    case 0xA8: start_adresse = kopf_A8[0].startadresse;
	           end_adresse = kopf_A8[0].endadresse;
      break;
	case 0xDC:
	  switch(kopf_DC[0].all_bytes[5])
	  {
		  case 1: start_adresse = kopf_DC[0].DC_Rahmen1.startadresse;
				  end_adresse = kopf_DC[0].DC_Rahmen1.endadresse;
                break;
		  case 2: start_adresse = kopf_DC[0].DC_Rahmen2.startadresse; 
				  end_adresse = kopf_DC[0].DC_Rahmen2.endadresse;
		        break;
		  case 3: start_adresse = kopf_DC[0].DC_Rahmen3.startadresse; 
				  end_adresse = kopf_DC[0].DC_Rahmen3.endadresse;
		        break;
		  case 4: start_adresse = kopf_DC[0].DC_Rahmen4.startadresse; 
				  end_adresse = kopf_DC[0].DC_Rahmen4.endadresse;
		        break;
		  case 5: start_adresse = kopf_DC[0].DC_Rahmen5.startadresse; 
				  end_adresse = kopf_DC[0].DC_Rahmen5.endadresse;
		        break;
		  case 6: start_adresse = kopf_DC[0].DC_Rahmen6.startadresse; 
				  end_adresse = kopf_DC[0].DC_Rahmen6.endadresse;
		        break;
		  case 7: start_adresse = kopf_DC[0].DC_Rahmen7.startadresse; 
				  end_adresse = kopf_DC[0].DC_Rahmen7.endadresse;
		        break;
		  case 8: start_adresse = kopf_DC[0].DC_Rahmen8.startadresse; 
				  end_adresse = kopf_DC[0].DC_Rahmen8.endadresse;
		        break;
	  }
      break;
  }

//fprintf(stderr," switch uvr_modus... \n");
  switch(uvr_modus)
  {
    case 0xD1: 
      anz_ds = anzahldatensaetze_D1(kopf_D1);
      break;
    case 0xA8: 
      anz_ds = anzahldatensaetze_A8(kopf_A8);
      break;
	case 0xDC:
      anz_ds = anzahldatensaetze_DC(kopf_DC);
#if DEBUG > 3
	  switch(kopf_DC[0].all_bytes[5])
	  {
	    case 1: print_endaddr = kopf_DC[0].DC_Rahmen1.endadresse[0]; 
		fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
		kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
		kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
		kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13]);
		break;
	    case 2: print_endaddr = kopf_DC[0].DC_Rahmen2.endadresse[0]; 
		fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
		kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
		kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
		kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14]);
		break;
	    case 3: print_endaddr = kopf_DC[0].DC_Rahmen3.endadresse[0]; 
		fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
		kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
		kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
		kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15]);
		break;
	    case 4: print_endaddr = kopf_DC[0].DC_Rahmen4.endadresse[0]; 
		fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
		kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
		kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
		kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
		kopf_DC[0].all_bytes[16]);
		break;
	    case 5: print_endaddr = kopf_DC[0].DC_Rahmen5.endadresse[0]; 
		fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
		kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
		kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
		kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
		kopf_DC[0].all_bytes[16],kopf_DC[0].all_bytes[17]);
		break;
	    case 6: print_endaddr = kopf_DC[0].DC_Rahmen6.endadresse[0]; 
		fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
		kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
		kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
		kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
		kopf_DC[0].all_bytes[16],kopf_DC[0].all_bytes[17],kopf_DC[0].all_bytes[18]);
		break;
	    case 7: print_endaddr = kopf_DC[0].DC_Rahmen7.endadresse[0]; 
		fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
		kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
		kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
		kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
		kopf_DC[0].all_bytes[16],kopf_DC[0].all_bytes[17],kopf_DC[0].all_bytes[18],kopf_DC[0].all_bytes[19]);
		break;
	    case 8: print_endaddr = kopf_DC[0].DC_Rahmen8.endadresse[0]; 
		fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
		kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
		kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
		kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
		kopf_DC[0].all_bytes[16],kopf_DC[0].all_bytes[17],kopf_DC[0].all_bytes[18],kopf_DC[0].all_bytes[19],kopf_DC[0].all_bytes[20]);
		break;
	  }
#endif
      break;
  }

#if  DEBUG>5
  fprintf(stderr, "  Errechnete Pruefsumme: %x\n", berechneKopfpruefziffer(kopf) ); printf(" empfangene Pruefsumme: %x\n",kopf[0].pruefsum);
  fprintf(stderr, "empfangen von DL/BL:\n Kennung: %X\n",kopf[0].kennung);
  fprintf(stderr, " Version: %X\n",kopf[0].version);
  fprintf(stderr, " Zeitstempel (hex): %x %x %x\n",kopf[0].zeitstempel[0],kopf[0].zeitstempel[1],kopf[0].zeitstempel[2]);
  fprintf(stderr, " Zeitstempel (int): %d %d %d\n",kopf[0].zeitstempel[0],kopf[0].zeitstempel[1],kopf[0].zeitstempel[2]);

  long secs = (long)(kopf[0].zeitstempel[0]) * 32768 + (long)(kopf[0].zeitstempel[1]) * 256 + (long)(kopf[0].zeitstempel[2]);
  long secsinv = (long)(kopf[0].zeitstempel[0]) + (long)(kopf[0].zeitstempel[1]) * 256 + (long)(kopf[0].zeitstempel[2]) * 32768;

  fprintf(stderr, " Zeitstempel : %ld  %ld\n", secs,secsinv);

  fprintf(stderr, " Satzlaenge: %x\n",kopf[0].satzlaenge);
  fprintf(stderr, " Startadresse: %x %x %x\n",kopf[0].startadresse[0],kopf[0].startadresse[1],kopf[0].startadresse[2]);
  fprintf(stderr, " Endadresse: %x %x %x\n",kopf[0].endadresse[0],kopf[0].endadresse[1],kopf[0].endadresse[2]);
#endif

  if ( uvr_modus == 0xD1 )
  {
    uvr_typ = kopf_D1[0].satzlaengeGeraet1; /* 0x5A -> UVR61-3; 0x76 -> UVR1611 */
    uvr_typ2 = kopf_D1[0].satzlaengeGeraet2; /* 0x5A -> UVR61-3; 0x76 -> UVR1611 */
  }
  else
  {
    uvr_typ = kopf_A8[0].satzlaengeGeraet1; /* 0x5A -> UVR61-3; 0x76 -> UVR1611 */
  }
  if ( uvr_modus == 0xDC )
  {
    uvr_typ = 0x76; /* CAN-Logging nur mit UVR1611 */  
  }
	  
  switch( anz_ds)
  {
    case -1: zeitstempel();
            fprintf(fp_varlogfile,"%s - %s -- Falschen Wert im Low-Byte Endadresse gelesen! Wert: %x\n",sDatum, sZeit,*end_adresse);
            return -1;
	case -2: zeitstempel();
            fprintf(fp_varlogfile,"%s - %s -- Keine Daten vorhanden.\n",sDatum, sZeit);
            return -1;
    default: printf(" Anzahl Datensaetze aus Kopfsatz: %i\n",anz_ds); break;
  }

  return anz_ds;
}

/* Funktion zum Testen */
void testfunktion(void)
{
  int result;
  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[256];

  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  write_erg=write(fd_serialport,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd_serialport,empfbuf,1);
  printf("Vom DL erhalten Version: %x\n",empfbuf[0]);

  sendbuf[0]=FWABFRAGE;    /* Senden der Firmware-Versionsabfrage */
  write_erg=write(fd_serialport,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd_serialport,empfbuf,1);
  printf("Vom DL erhalten Version FW: %x\n",empfbuf[0]);

  sendbuf[0]=MODEABFRAGE;    /* Senden der Modus-abfrage */
  write_erg=write(fd_serialport,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd_serialport,empfbuf,1);
  printf("Vom DL erhalten Modus: %x\n",empfbuf[0]);
}

/* Kopieren der Daten (UVR1611) in die Winsol-Format Struktur */
int copy_UVR2winsol_1611(u_DS_UVR1611_UVR61_3 *dsatz_uvr1611, DS_Winsol  *dsatz_winsol )
{
  BYTES_LONG byteslong_mlstg1, byteslong_mlstg2;

  dsatz_winsol[0].tag = dsatz_uvr1611[0].DS_UVR1611.datum_zeit.tag ;
  dsatz_winsol[0].std = dsatz_uvr1611[0].DS_UVR1611.datum_zeit.std ;
  dsatz_winsol[0].min = dsatz_uvr1611[0].DS_UVR1611.datum_zeit.min ;
  dsatz_winsol[0].sek = dsatz_uvr1611[0].DS_UVR1611.datum_zeit.sek ;
  dsatz_winsol[0].ausgbyte1 = dsatz_uvr1611[0].DS_UVR1611.ausgbyte1 ;
  dsatz_winsol[0].ausgbyte2 = dsatz_uvr1611[0].DS_UVR1611.ausgbyte2 ;
  dsatz_winsol[0].dza[0] = dsatz_uvr1611[0].DS_UVR1611.dza[0] ;
  dsatz_winsol[0].dza[1] = dsatz_uvr1611[0].DS_UVR1611.dza[1] ;
  dsatz_winsol[0].dza[2] = dsatz_uvr1611[0].DS_UVR1611.dza[2] ;
  dsatz_winsol[0].dza[3] = dsatz_uvr1611[0].DS_UVR1611.dza[3] ;

  int ii=0;
  for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_uvr1611[0].DS_UVR1611.sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_uvr1611[0].DS_UVR1611.sensT[ii][1] ;
    }
  dsatz_winsol[0].wmzaehler_reg = dsatz_uvr1611[0].DS_UVR1611.wmzaehler_reg ;

  if ( dsatz_uvr1611[0].DS_UVR1611.mlstg1[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[3]
    +256*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[2]
    +(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[1])-65536)
    -((float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[0]*10)/256);
    byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[3]
    +256*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[2]
    +(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[1])
    +((float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[0]*10)/256);

  dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
  dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
  dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
  dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
  dsatz_winsol[0].kwh1[0] = dsatz_uvr1611[0].DS_UVR1611.kwh1[0] ;
  dsatz_winsol[0].kwh1[1] = dsatz_uvr1611[0].DS_UVR1611.kwh1[1] ;
  dsatz_winsol[0].mwh1[0] = dsatz_uvr1611[0].DS_UVR1611.mwh1[0] ;
  dsatz_winsol[0].mwh1[1] = dsatz_uvr1611[0].DS_UVR1611.mwh1[1] ;

  if ( dsatz_uvr1611[0].DS_UVR1611.mlstg2[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[3]
    +256*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[2]
    +(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[1])-65536)
    -((float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[0]*10)/256);
    byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[3]
    +256*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[2]
    +(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[1])
    +((float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[0]*10)/256);

  dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
  dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
  dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
  dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
  dsatz_winsol[0].kwh2[0] = dsatz_uvr1611[0].DS_UVR1611.kwh2[0] ;
  dsatz_winsol[0].kwh2[1] = dsatz_uvr1611[0].DS_UVR1611.kwh2[1] ;
  dsatz_winsol[0].mwh2[0] = dsatz_uvr1611[0].DS_UVR1611.mwh2[0] ;
  dsatz_winsol[0].mwh2[1] = dsatz_uvr1611[0].DS_UVR1611.mwh2[1] ;

  return 1;
}

/* Kopieren der Daten (UVR1611 - CAN-Logging) in die Winsol-Format Struktur */
int copy_UVR2winsol_1611_CAN(s_DS_CAN *dsatz_uvr1611, DS_Winsol  *dsatz_winsol )
{
  BYTES_LONG byteslong_mlstg1, byteslong_mlstg2;

  dsatz_winsol[0].tag = dsatz_uvr1611[0].datum_zeit.tag ;
  dsatz_winsol[0].std = dsatz_uvr1611[0].datum_zeit.std ;
  dsatz_winsol[0].min = dsatz_uvr1611[0].datum_zeit.min ;
  dsatz_winsol[0].sek = dsatz_uvr1611[0].datum_zeit.sek ;
  dsatz_winsol[0].ausgbyte1 = dsatz_uvr1611[0].ausgbyte1 ;
  dsatz_winsol[0].ausgbyte2 = dsatz_uvr1611[0].ausgbyte2 ;
  dsatz_winsol[0].dza[0] = dsatz_uvr1611[0].dza[0] ;
  dsatz_winsol[0].dza[1] = dsatz_uvr1611[0].dza[1] ;
  dsatz_winsol[0].dza[2] = dsatz_uvr1611[0].dza[2] ;
  dsatz_winsol[0].dza[3] = dsatz_uvr1611[0].dza[3] ;

  int ii=0;
  for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_uvr1611[0].sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_uvr1611[0].sensT[ii][1] ;
    }
  dsatz_winsol[0].wmzaehler_reg = dsatz_uvr1611[0].wmzaehler_reg ;

  if ( dsatz_uvr1611[0].mlstg1[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_uvr1611[0].mlstg1[3]
    +256*(float)dsatz_uvr1611[0].mlstg1[2]
    +(float)dsatz_uvr1611[0].mlstg1[1])-65536)
    -((float)dsatz_uvr1611[0].mlstg1[0]*10)/256);
    byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_uvr1611[0].mlstg1[3]
    +256*(float)dsatz_uvr1611[0].mlstg1[2]
    +(float)dsatz_uvr1611[0].mlstg1[1])
    +((float)dsatz_uvr1611[0].mlstg1[0]*10)/256);

  dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
  dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
  dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
  dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
  dsatz_winsol[0].kwh1[0] = dsatz_uvr1611[0].kwh1[0] ;
  dsatz_winsol[0].kwh1[1] = dsatz_uvr1611[0].kwh1[1] ;
  dsatz_winsol[0].mwh1[0] = dsatz_uvr1611[0].mwh1[0] ;
  dsatz_winsol[0].mwh1[1] = dsatz_uvr1611[0].mwh1[1] ;

  if ( dsatz_uvr1611[0].mlstg2[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_uvr1611[0].mlstg2[3]
    +256*(float)dsatz_uvr1611[0].mlstg2[2]
    +(float)dsatz_uvr1611[0].mlstg2[1])-65536)
    -((float)dsatz_uvr1611[0].mlstg2[0]*10)/256);
    byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_uvr1611[0].mlstg2[3]
    +256*(float)dsatz_uvr1611[0].mlstg2[2]
    +(float)dsatz_uvr1611[0].mlstg2[1])
    +((float)dsatz_uvr1611[0].mlstg2[0]*10)/256);

  dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
  dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
  dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
  dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
  dsatz_winsol[0].kwh2[0] = dsatz_uvr1611[0].kwh2[0] ;
  dsatz_winsol[0].kwh2[1] = dsatz_uvr1611[0].kwh2[1] ;
  dsatz_winsol[0].mwh2[0] = dsatz_uvr1611[0].mwh2[0] ;
  dsatz_winsol[0].mwh2[1] = dsatz_uvr1611[0].mwh2[1] ;

  return 1;
}

/* Kopieren der Daten (UVR61-3) in die Winsol-Format Struktur */
int copy_UVR2winsol_61_3(u_DS_UVR1611_UVR61_3 *dsatz_uvr61_3, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3 )
{
  dsatz_winsol_uvr61_3[0].tag = dsatz_uvr61_3[0].DS_UVR61_3.datum_zeit.tag ;
  dsatz_winsol_uvr61_3[0].std = dsatz_uvr61_3[0].DS_UVR61_3.datum_zeit.std ;
  dsatz_winsol_uvr61_3[0].min = dsatz_uvr61_3[0].DS_UVR61_3.datum_zeit.min ;
  dsatz_winsol_uvr61_3[0].sek = dsatz_uvr61_3[0].DS_UVR61_3.datum_zeit.sek ;
  dsatz_winsol_uvr61_3[0].ausgbyte1 = dsatz_uvr61_3[0].DS_UVR61_3.ausgbyte1 ;
  dsatz_winsol_uvr61_3[0].dza = dsatz_uvr61_3[0].DS_UVR61_3.dza ;
  dsatz_winsol_uvr61_3[0].ausg_analog = dsatz_uvr61_3[0].DS_UVR61_3.ausg_analog;

  int ii=0;
  for (ii=0;ii<6;ii++)
    {
      dsatz_winsol_uvr61_3[0].sensT[ii][0] = dsatz_uvr61_3[0].DS_UVR61_3.sensT[ii][0] ;
      dsatz_winsol_uvr61_3[0].sensT[ii][1] = dsatz_uvr61_3[0].DS_UVR61_3.sensT[ii][1] ;
    }
  dsatz_winsol_uvr61_3[0].wmzaehler_reg = dsatz_uvr61_3[0].DS_UVR61_3.wmzaehler_reg ;
  dsatz_winsol_uvr61_3[0].volstrom[0] = dsatz_uvr61_3[0].DS_UVR61_3.volstrom[0] ;
  dsatz_winsol_uvr61_3[0].volstrom[1] = dsatz_uvr61_3[0].DS_UVR61_3.volstrom[1] ;
  dsatz_winsol_uvr61_3[0].mlstg[0] = dsatz_uvr61_3[0].DS_UVR61_3.mlstg1[0] ;
  dsatz_winsol_uvr61_3[0].mlstg[1] = dsatz_uvr61_3[0].DS_UVR61_3.mlstg1[1] ;
  dsatz_winsol_uvr61_3[0].mlstg[2] = dsatz_uvr61_3[0].DS_UVR61_3.mlstg1[2] ;
  dsatz_winsol_uvr61_3[0].mlstg[3] = dsatz_uvr61_3[0].DS_UVR61_3.mlstg1[3] ;
  dsatz_winsol_uvr61_3[0].kwh[0] = dsatz_uvr61_3[0].DS_UVR61_3.kwh1[0] ;
  dsatz_winsol_uvr61_3[0].kwh[1] = dsatz_uvr61_3[0].DS_UVR61_3.kwh1[1] ;
  dsatz_winsol_uvr61_3[0].mwh[0] = dsatz_uvr61_3[0].DS_UVR61_3.mwh1[0] ;
  dsatz_winsol_uvr61_3[0].mwh[1] = dsatz_uvr61_3[0].DS_UVR61_3.mwh1[1] ;
  dsatz_winsol_uvr61_3[0].mwh[2] = dsatz_uvr61_3[0].DS_UVR61_3.mwh1[2] ;
  dsatz_winsol_uvr61_3[0].mwh[3] = dsatz_uvr61_3[0].DS_UVR61_3.mwh1[3] ;

  for (ii=0;ii<4;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt1[ii] = 0x0;
    }
  for (ii=0;ii<25;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt2[ii] = 0x0;
    }

  return 1;
}

/* Kopieren der Daten UVR1611(Modus 0xD1 - 2DL) in Winsol-Format-Struktur */
int copy_UVR2winsol_D1_1611(u_modus_D1 *dsatz_modus_d1, DS_Winsol *dsatz_winsol , int geraet)
{
  BYTES_LONG byteslong_mlstg1, byteslong_mlstg2;

  if ( geraet == 1 )
  {
    dsatz_winsol[0].tag = dsatz_modus_d1[0].DS_1611_1611.datum_zeit.tag ;
    dsatz_winsol[0].std = dsatz_modus_d1[0].DS_1611_1611.datum_zeit.std ;
    dsatz_winsol[0].min = dsatz_modus_d1[0].DS_1611_1611.datum_zeit.min ;
    dsatz_winsol[0].sek = dsatz_modus_d1[0].DS_1611_1611.datum_zeit.sek ;
    dsatz_winsol[0].ausgbyte1 = dsatz_modus_d1[0].DS_1611_1611.ausgbyte1 ;
    dsatz_winsol[0].ausgbyte2 = dsatz_modus_d1[0].DS_1611_1611.ausgbyte2 ;
    dsatz_winsol[0].dza[0] = dsatz_modus_d1[0].DS_1611_1611.dza[0] ;
    dsatz_winsol[0].dza[1] = dsatz_modus_d1[0].DS_1611_1611.dza[1] ;
    dsatz_winsol[0].dza[2] = dsatz_modus_d1[0].DS_1611_1611.dza[2] ;
    dsatz_winsol[0].dza[3] = dsatz_modus_d1[0].DS_1611_1611.dza[3] ;

    int ii=0;
    for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_modus_d1[0].DS_1611_1611.sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_modus_d1[0].DS_1611_1611.sensT[ii][1] ;
    }
    dsatz_winsol[0].wmzaehler_reg = dsatz_modus_d1[0].DS_1611_1611.wmzaehler_reg ;

    if ( dsatz_modus_d1[0].DS_1611_1611.mlstg1[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[1])-65536)
      -((float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[0]*10)/256);
      byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[1])
      +((float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[0]*10)/256);

    dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
    dsatz_winsol[0].kwh1[0] = dsatz_modus_d1[0].DS_1611_1611.kwh1[0] ;
    dsatz_winsol[0].kwh1[1] = dsatz_modus_d1[0].DS_1611_1611.kwh1[1] ;
    dsatz_winsol[0].mwh1[0] = dsatz_modus_d1[0].DS_1611_1611.mwh1[0] ;
    dsatz_winsol[0].mwh1[1] = dsatz_modus_d1[0].DS_1611_1611.mwh1[1] ;

    if ( dsatz_modus_d1[0].DS_1611_1611.mlstg2[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[1])-65536)
      -((float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[0]*10)/256);
      byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[1])
      +((float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[0]*10)/256);

    dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
    dsatz_winsol[0].kwh2[0] = dsatz_modus_d1[0].DS_1611_1611.kwh2[0] ;
    dsatz_winsol[0].kwh2[1] = dsatz_modus_d1[0].DS_1611_1611.kwh2[1] ;
    dsatz_winsol[0].mwh2[0] = dsatz_modus_d1[0].DS_1611_1611.mwh2[0] ;
    dsatz_winsol[0].mwh2[1] = dsatz_modus_d1[0].DS_1611_1611.mwh2[1] ;
  }
  else if (uvr_typ == UVR1611)
  {
    dsatz_winsol[0].tag = dsatz_modus_d1[0].DS_1611_1611.Z_datum_zeit.tag ;
    dsatz_winsol[0].std = dsatz_modus_d1[0].DS_1611_1611.Z_datum_zeit.std ;
    dsatz_winsol[0].min = dsatz_modus_d1[0].DS_1611_1611.Z_datum_zeit.min ;
    dsatz_winsol[0].sek = dsatz_modus_d1[0].DS_1611_1611.Z_datum_zeit.sek ;
    dsatz_winsol[0].ausgbyte1 = dsatz_modus_d1[0].DS_1611_1611.Z_ausgbyte1 ;
    dsatz_winsol[0].ausgbyte2 = dsatz_modus_d1[0].DS_1611_1611.Z_ausgbyte2 ;
    dsatz_winsol[0].dza[0] = dsatz_modus_d1[0].DS_1611_1611.Z_dza[0] ;
    dsatz_winsol[0].dza[1] = dsatz_modus_d1[0].DS_1611_1611.Z_dza[1] ;
    dsatz_winsol[0].dza[2] = dsatz_modus_d1[0].DS_1611_1611.Z_dza[2] ;
    dsatz_winsol[0].dza[3] = dsatz_modus_d1[0].DS_1611_1611.Z_dza[3] ;

    int ii=0;
    for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_modus_d1[0].DS_1611_1611.Z_sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_modus_d1[0].DS_1611_1611.Z_sensT[ii][1] ;
    }
    dsatz_winsol[0].wmzaehler_reg = dsatz_modus_d1[0].DS_1611_1611.Z_wmzaehler_reg ;

    if ( dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[1])-65536)
      -((float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[0]*10)/256);
      byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[1])
      +((float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[0]*10)/256);

    dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
    dsatz_winsol[0].kwh1[0] = dsatz_modus_d1[0].DS_1611_1611.Z_kwh1[0] ;
    dsatz_winsol[0].kwh1[1] = dsatz_modus_d1[0].DS_1611_1611.Z_kwh1[1] ;
    dsatz_winsol[0].mwh1[0] = dsatz_modus_d1[0].DS_1611_1611.Z_mwh1[0] ;
    dsatz_winsol[0].mwh1[1] = dsatz_modus_d1[0].DS_1611_1611.Z_mwh1[1] ;

    if ( dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[1])-65536)
      -((float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[0]*10)/256);
      byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[1])
      +((float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[0]*10)/256);

    dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
    dsatz_winsol[0].kwh2[0] = dsatz_modus_d1[0].DS_1611_1611.Z_kwh2[0] ;
    dsatz_winsol[0].kwh2[1] = dsatz_modus_d1[0].DS_1611_1611.Z_kwh2[1] ;
    dsatz_winsol[0].mwh2[0] = dsatz_modus_d1[0].DS_1611_1611.Z_mwh2[0] ;
    dsatz_winsol[0].mwh2[1] = dsatz_modus_d1[0].DS_1611_1611.Z_mwh2[1] ;
  }
  else if (uvr_typ == UVR61_3)
  {
    dsatz_winsol[0].tag = dsatz_modus_d1[0].DS_61_3_1611.Z_datum_zeit.tag ;
    dsatz_winsol[0].std = dsatz_modus_d1[0].DS_61_3_1611.Z_datum_zeit.std ;
    dsatz_winsol[0].min = dsatz_modus_d1[0].DS_61_3_1611.Z_datum_zeit.min ;
    dsatz_winsol[0].sek = dsatz_modus_d1[0].DS_61_3_1611.Z_datum_zeit.sek ;
    dsatz_winsol[0].ausgbyte1 = dsatz_modus_d1[0].DS_61_3_1611.Z_ausgbyte1 ;
    dsatz_winsol[0].ausgbyte2 = dsatz_modus_d1[0].DS_61_3_1611.Z_ausgbyte2 ;
    dsatz_winsol[0].dza[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_dza[0] ;
    dsatz_winsol[0].dza[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_dza[1] ;
    dsatz_winsol[0].dza[2] = dsatz_modus_d1[0].DS_61_3_1611.Z_dza[2] ;
    dsatz_winsol[0].dza[3] = dsatz_modus_d1[0].DS_61_3_1611.Z_dza[3] ;

    int ii=0;
    for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_modus_d1[0].DS_61_3_1611.Z_sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_modus_d1[0].DS_61_3_1611.Z_sensT[ii][1] ;
    }
    dsatz_winsol[0].wmzaehler_reg = dsatz_modus_d1[0].DS_61_3_1611.Z_wmzaehler_reg ;

    if ( dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[1])-65536)
      -((float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[0]*10)/256);
      byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[1])
      +((float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[0]*10)/256);

    dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
    dsatz_winsol[0].kwh1[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_kwh1[0] ;
    dsatz_winsol[0].kwh1[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_kwh1[1] ;
    dsatz_winsol[0].mwh1[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_mwh1[0] ;
    dsatz_winsol[0].mwh1[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_mwh1[1] ;

    if ( dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[1])-65536)
      -((float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[0]*10)/256);
      byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[1])
      +((float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[0]*10)/256);

    dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
    dsatz_winsol[0].kwh2[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_kwh2[0] ;
    dsatz_winsol[0].kwh2[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_kwh2[1] ;
    dsatz_winsol[0].mwh2[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_mwh2[0] ;
    dsatz_winsol[0].mwh2[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_mwh2[1] ;
  }

  return 1;
}


/* Kopieren der Daten UVR61-3(Modus 0xD1 - 2DL) in Winsol-Format-Struktur */
int copy_UVR2winsol_D1_61_3(u_modus_D1 *dsatz_modus_d1, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3 , int geraet)
{
  if ( geraet == 1 )
  {
    dsatz_winsol_uvr61_3[0].tag = dsatz_modus_d1[0].DS_61_3_61_3.datum_zeit.tag ;
    dsatz_winsol_uvr61_3[0].std = dsatz_modus_d1[0].DS_61_3_61_3.datum_zeit.std ;
    dsatz_winsol_uvr61_3[0].min = dsatz_modus_d1[0].DS_61_3_61_3.datum_zeit.min ;
    dsatz_winsol_uvr61_3[0].sek = dsatz_modus_d1[0].DS_61_3_61_3.datum_zeit.sek ;
    dsatz_winsol_uvr61_3[0].ausgbyte1 = dsatz_modus_d1[0].DS_61_3_61_3.ausgbyte1 ;
    dsatz_winsol_uvr61_3[0].dza = dsatz_modus_d1[0].DS_61_3_61_3.dza ;
    dsatz_winsol_uvr61_3[0].ausg_analog = dsatz_modus_d1[0].DS_61_3_61_3.ausg_analog;

    int ii=0;
    for (ii=0;ii<6;ii++)
    {
      dsatz_winsol_uvr61_3[0].sensT[ii][0] = dsatz_modus_d1[0].DS_61_3_61_3.sensT[ii][0] ;
      dsatz_winsol_uvr61_3[0].sensT[ii][1] = dsatz_modus_d1[0].DS_61_3_61_3.sensT[ii][1] ;
    }
    dsatz_winsol_uvr61_3[0].wmzaehler_reg = dsatz_modus_d1[0].DS_61_3_61_3.wmzaehler_reg ;
    dsatz_winsol_uvr61_3[0].volstrom[0] = dsatz_modus_d1[0].DS_61_3_61_3.volstrom[0] ;
    dsatz_winsol_uvr61_3[0].volstrom[1] = dsatz_modus_d1[0].DS_61_3_61_3.volstrom[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[0] = dsatz_modus_d1[0].DS_61_3_61_3.mlstg1[0] ;
    dsatz_winsol_uvr61_3[0].mlstg[1] = dsatz_modus_d1[0].DS_61_3_61_3.mlstg1[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[2] = dsatz_modus_d1[0].DS_61_3_61_3.mlstg1[2] ;
    dsatz_winsol_uvr61_3[0].mlstg[3] = dsatz_modus_d1[0].DS_61_3_61_3.mlstg1[3] ;
    dsatz_winsol_uvr61_3[0].kwh[0] = dsatz_modus_d1[0].DS_61_3_61_3.kwh1[0] ;
    dsatz_winsol_uvr61_3[0].kwh[1] = dsatz_modus_d1[0].DS_61_3_61_3.kwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[0] = dsatz_modus_d1[0].DS_61_3_61_3.mwh1[0] ;
    dsatz_winsol_uvr61_3[0].mwh[1] = dsatz_modus_d1[0].DS_61_3_61_3.mwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[2] = dsatz_modus_d1[0].DS_61_3_61_3.mwh1[2] ;
    dsatz_winsol_uvr61_3[0].mwh[3] = dsatz_modus_d1[0].DS_61_3_61_3.mwh1[3] ;

    for (ii=0;ii<4;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt1[ii] = 0x0;
    }
    for (ii=0;ii<25;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt2[ii] = 0x0;
    }
  }
  else if (uvr_typ == UVR61_3)
  {
    dsatz_winsol_uvr61_3[0].tag = dsatz_modus_d1[0].DS_61_3_61_3.Z_datum_zeit.tag ;
    dsatz_winsol_uvr61_3[0].std = dsatz_modus_d1[0].DS_61_3_61_3.Z_datum_zeit.std ;
    dsatz_winsol_uvr61_3[0].min = dsatz_modus_d1[0].DS_61_3_61_3.Z_datum_zeit.min ;
    dsatz_winsol_uvr61_3[0].sek = dsatz_modus_d1[0].DS_61_3_61_3.Z_datum_zeit.sek ;
    dsatz_winsol_uvr61_3[0].ausgbyte1 = dsatz_modus_d1[0].DS_61_3_61_3.Z_ausgbyte1 ;
    dsatz_winsol_uvr61_3[0].dza = dsatz_modus_d1[0].DS_61_3_61_3.Z_dza ;
    dsatz_winsol_uvr61_3[0].ausg_analog = dsatz_modus_d1[0].DS_61_3_61_3.Z_ausg_analog;

    int ii=0;
    for (ii=0;ii<6;ii++)
    {
      dsatz_winsol_uvr61_3[0].sensT[ii][0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_sensT[ii][0] ;
      dsatz_winsol_uvr61_3[0].sensT[ii][1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_sensT[ii][1] ;
    }
    dsatz_winsol_uvr61_3[0].wmzaehler_reg = dsatz_modus_d1[0].DS_61_3_61_3.Z_wmzaehler_reg ;
    dsatz_winsol_uvr61_3[0].volstrom[0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_volstrom[0] ;
    dsatz_winsol_uvr61_3[0].volstrom[1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_volstrom[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mlstg1[0] ;
    dsatz_winsol_uvr61_3[0].mlstg[1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mlstg1[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[2] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mlstg1[2] ;
    dsatz_winsol_uvr61_3[0].mlstg[3] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mlstg1[3] ;
    dsatz_winsol_uvr61_3[0].kwh[0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_kwh1[0] ;
    dsatz_winsol_uvr61_3[0].kwh[1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_kwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mwh1[0] ;
    dsatz_winsol_uvr61_3[0].mwh[1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[2] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mwh1[2] ;
    dsatz_winsol_uvr61_3[0].mwh[3] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mwh1[3] ;

    for (ii=0;ii<4;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt1[ii] = 0x0;
    }
    for (ii=0;ii<25;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt2[ii] = 0x0;
    }
  }
  else if (uvr_typ == UVR1611)
  {
    dsatz_winsol_uvr61_3[0].tag = dsatz_modus_d1[0].DS_1611_61_3.Z_datum_zeit.tag ;
    dsatz_winsol_uvr61_3[0].std = dsatz_modus_d1[0].DS_1611_61_3.Z_datum_zeit.std ;
    dsatz_winsol_uvr61_3[0].min = dsatz_modus_d1[0].DS_1611_61_3.Z_datum_zeit.min ;
    dsatz_winsol_uvr61_3[0].sek = dsatz_modus_d1[0].DS_1611_61_3.Z_datum_zeit.sek ;
    dsatz_winsol_uvr61_3[0].ausgbyte1 = dsatz_modus_d1[0].DS_1611_61_3.Z_ausgbyte1 ;
    dsatz_winsol_uvr61_3[0].dza = dsatz_modus_d1[0].DS_1611_61_3.Z_dza ;
    dsatz_winsol_uvr61_3[0].ausg_analog = dsatz_modus_d1[0].DS_1611_61_3.Z_ausg_analog;

    int ii=0;
    for (ii=0;ii<6;ii++)
    {
      dsatz_winsol_uvr61_3[0].sensT[ii][0] = dsatz_modus_d1[0].DS_1611_61_3.Z_sensT[ii][0] ;
      dsatz_winsol_uvr61_3[0].sensT[ii][1] = dsatz_modus_d1[0].DS_1611_61_3.Z_sensT[ii][1] ;
    }
    dsatz_winsol_uvr61_3[0].wmzaehler_reg = dsatz_modus_d1[0].DS_1611_61_3.Z_wmzaehler_reg ;
    dsatz_winsol_uvr61_3[0].volstrom[0] = dsatz_modus_d1[0].DS_1611_61_3.Z_volstrom[0] ;
    dsatz_winsol_uvr61_3[0].volstrom[1] = dsatz_modus_d1[0].DS_1611_61_3.Z_volstrom[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[0] = dsatz_modus_d1[0].DS_1611_61_3.Z_mlstg1[0] ;
    dsatz_winsol_uvr61_3[0].mlstg[1] = dsatz_modus_d1[0].DS_1611_61_3.Z_mlstg1[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[2] = dsatz_modus_d1[0].DS_1611_61_3.Z_mlstg1[2] ;
    dsatz_winsol_uvr61_3[0].mlstg[3] = dsatz_modus_d1[0].DS_1611_61_3.Z_mlstg1[3] ;
    dsatz_winsol_uvr61_3[0].kwh[0] = dsatz_modus_d1[0].DS_1611_61_3.Z_kwh1[0] ;
    dsatz_winsol_uvr61_3[0].kwh[1] = dsatz_modus_d1[0].DS_1611_61_3.Z_kwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[0] = dsatz_modus_d1[0].DS_1611_61_3.Z_mwh1[0] ;
    dsatz_winsol_uvr61_3[0].mwh[1] = dsatz_modus_d1[0].DS_1611_61_3.Z_mwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[2] = dsatz_modus_d1[0].DS_1611_61_3.Z_mwh1[2] ;
    dsatz_winsol_uvr61_3[0].mwh[3] = dsatz_modus_d1[0].DS_1611_61_3.Z_mwh1[3] ;

    for (ii=0;ii<4;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt1[ii] = 0x0;
    }
    for (ii=0;ii<25;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt2[ii] = 0x0;
    }
  }

  return 1;
}

/* Ausgabe der Daten auf Bildschirm  */
void print_dsatz_uvr1611_content(const u_DS_UVR1611_UVR61_3 *dsatz_uvr1611)
{
  printf(" Datensatz start\n");
  printf("Zeitstempel: %X %X %X\n",dsatz_uvr1611[0].DS_UVR1611.zeitstempel[1],dsatz_uvr1611[0].DS_UVR1611.zeitstempel[2],dsatz_uvr1611[0].DS_UVR1611.zeitstempel[3]);

  printf("Zeit/Datum: %02d:%02d:%02d / %02d.%02d.200%d \n",
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.std,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.min,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.sek,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.tag,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.monat,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.jahr);

  int ii=0;
  for(ii=0;ii<12;ii++)
    {
      int k=0;
      printf("Temperaturen ");
      for(k=0;k<4;k++)
  printf(" t%d: %x%X ",ii+k+1,
         dsatz_uvr1611[0].DS_UVR1611.sensT[ii+k][0],
         dsatz_uvr1611[0].DS_UVR1611.sensT[ii+k][1]);

      printf("\n Temperaturen ");
      for(k=0;k<4;k++)
  printf(" t%d: %g ",ii+k+1,
         berechnetemp(dsatz_uvr1611[0].DS_UVR1611.sensT[ii][0],
          dsatz_uvr1611[0].DS_UVR1611.sensT[ii][1]));
      printf("\n");
    }
  printf("Ausgangsbytes (1) %X, (2)%X\n",dsatz_uvr1611[0].DS_UVR1611.ausgbyte1,dsatz_uvr1611[0].DS_UVR1611.ausgbyte2);
  printf("Drehzahlstufen (1) %X, (2) %X, (6) %X, (7) %X\n",dsatz_uvr1611[0].DS_UVR1611.dza[0],dsatz_uvr1611[0].DS_UVR1611.dza[1],dsatz_uvr1611[0].DS_UVR1611.dza[2],dsatz_uvr1611[0].DS_UVR1611.dza[3]);
  printf("Waermemengenzaehler-bit: %X\n",dsatz_uvr1611[0].DS_UVR1611.wmzaehler_reg);
  printf("(1) Momentleistung %X%x%X%x, Kwh %X%x, MWh %X%x\n",dsatz_uvr1611[0].DS_UVR1611.mlstg1[1],dsatz_uvr1611[0].DS_UVR1611.mlstg1[2],dsatz_uvr1611[0].DS_UVR1611.mlstg1[3],dsatz_uvr1611[0].DS_UVR1611.mlstg1[4],dsatz_uvr1611[0].DS_UVR1611.kwh1[1],dsatz_uvr1611[0].DS_UVR1611.kwh1[2],dsatz_uvr1611[0].DS_UVR1611.mwh1[1],dsatz_uvr1611[0].DS_UVR1611.mwh1[2]);
  printf("(2) Momentleistung %X%x%X%x, Kwh %X%x, MWh %X%x\n",dsatz_uvr1611[0].DS_UVR1611.mlstg2[1],dsatz_uvr1611[0].DS_UVR1611.mlstg2[2],dsatz_uvr1611[0].DS_UVR1611.mlstg2[3],dsatz_uvr1611[0].DS_UVR1611.mlstg2[4],dsatz_uvr1611[0].DS_UVR1611.kwh2[1],dsatz_uvr1611[0].DS_UVR1611.kwh2[2],dsatz_uvr1611[0].DS_UVR1611.mwh2[1],dsatz_uvr1611[0].DS_UVR1611.mwh2[2]);

  printf("Zeit/Datum: %02d:%02d:%02d / %02d.%02d.200%d \n",
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.std,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.min,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.sek,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.tag,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.monat,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.jahr);

  printf("Zeitstempel: %X %X %X\n",dsatz_uvr1611[0].DS_UVR1611.zeitstempel[1],dsatz_uvr1611[0].DS_UVR1611.zeitstempel[2],dsatz_uvr1611[0].DS_UVR1611.zeitstempel[3]);
  printf(" Datensatz ende\n");
}

/* Daten vom DL lesen - 1DL-Modus */
int datenlesen_A8(int anz_datensaetze)
{
  unsigned modTeiler;
  int i, merk_i, fehlerhafte_ds, result, lowbyte, middlebyte, merkmiddlebyte, tmp_erg = 0;
  int Bytes_for_0xA8 = 65, monatswechsel = 0;
  u_DS_UVR1611_UVR61_3 u_dsatz_uvr[1];
  DS_Winsol dsatz_winsol[1];
  DS_Winsol *puffer_dswinsol = &dsatz_winsol[0];
  DS_Winsol_UVR61_3 dsatz_winsol_uvr61_3[1];
  DS_Winsol_UVR61_3 *puffer_dswinsol_uvr61_3 = &dsatz_winsol_uvr61_3[0];
  UCHAR pruefsumme = 0, merk_monat = 0;
  UCHAR sendbuf[6];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[6];

  modTeiler = 0x100;
  i = 0; /* Gesamtdurchlaufzaehler mit 0 initialisiert */
  merk_i = 0; /* Bei falscher Pruefziffer den Datensatz bis zu fuenfmal wiederholt lesen */
  fehlerhafte_ds = 0; /* Anzahl fehlerhaft gelesener Datensaetze mit 0 initialisiert */
  lowbyte = 0;
  middlebyte = 0;
  merkmiddlebyte = middlebyte;

fprintf(stderr, " CAN-Logging: TEST ...\n");
  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  if (usb_zugriff)
  {
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd_serialport,empfbuf,1);
  }
  if (ip_zugriff)
  {
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
    }  /* if (!ip_first) */
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
  }

  /* fuellen des Sendebuffer - 6 Byte */
  sendbuf[0] = DATENBEREICHLESEN;
  // Startadressen von Kopfsatzlesen:
  sendbuf[1] = *start_adresse;     /* Beginn des Datenbereiches (low vor high) */
  sendbuf[2] = *(start_adresse+1);
  sendbuf[3] = *(start_adresse+2);
  sendbuf[4] = 0x01;  /* Anzahl der zu lesenden Rahmen */

 //#if DEBUG
fprintf(stderr," Startadresse: %x %x %x\n",sendbuf[1],sendbuf[2],sendbuf[3]);
//#endif
  for(;i<anz_datensaetze;i++)
  {
    sendbuf[5] = (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4]) % modTeiler;  /* Pruefziffer */

    if (usb_zugriff)
    {
      write_erg=write(fd_serialport,sendbuf,6);
      if (write_erg == 6)    /* Lesen der Antwort*/
        // result=read(fd_serialport, dsatz_uvr1611,65);
        result=read(fd_serialport, u_dsatz_uvr,Bytes_for_0xA8);
    }
    if (ip_zugriff)
    {
      if (!ip_first)
      {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup();
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup();
          return 3;
        }
       }  /* if (!ip_first) */

       write_erg=send(sock,sendbuf,6,0);
       if (write_erg == 6)    /* Lesen der Antwort */
          result  = recv(sock, u_dsatz_uvr,Bytes_for_0xA8,0);

        // result  = recv(sock, dsatz_uvr1611,65,0);
    } /* if (ip_zugriff) */

//**************************************************************************************** !!!!!!!!
    if (uvr_typ == UVR1611)
      pruefsumme = berechnepruefziffer_uvr1611(u_dsatz_uvr);
    if (uvr_typ == UVR61_3)
      pruefsumme = berechnepruefziffer_uvr61_3(u_dsatz_uvr);
#if DEBUG > 3
  if (uvr_typ == UVR1611)
      fprintf(stderr, "%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR1611.pruefsum,pruefsumme);
  if (uvr_typ == UVR61_3)
      fprintf(stderr, "%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR61_3.pruefsum,pruefsumme);
#endif

    if (u_dsatz_uvr[0].DS_UVR1611.pruefsum == pruefsumme || u_dsatz_uvr[0].DS_UVR61_3.pruefsum == pruefsumme)
    {  /*Aenderung: 02.09.06 - Hochzaehlen der Startadresse erst dann, wenn korrekt gelesen wurde (eventuell endlosschleife?) */
#if DEBUG > 4
    print_dsatz_uvr1611_content(u_dsatz_uvr);
#endif
      if ( i == 0 ) /* erster Datenssatz wurde gelesen - Logfile oeffnen / erstellen */
      {
        if (uvr_typ == UVR1611)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat,u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr));
          merk_monat = u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat;
        }
        if (uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr));
          merk_monat = u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat;
        }
        if ( tmp_erg == 0 )
        {
          printf("Der Logfile-Name konnte nicht erzeugt werden!");
          exit(-1);
        }
        else
        {
          if ( open_logfile(LogFileName, 1) == -1 )
          {
            printf("Das LogFile kann nicht geoeffnet werden!\n");
            exit(-1);
          }
        }
      }
      /* Hat der Monat gewechselt? Wenn ja, neues LogFile erstellen. */
      if ( uvr_typ == UVR1611 && merk_monat != u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat )
        monatswechsel = 1;
      if ( uvr_typ == UVR61_3 && merk_monat != u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat )
        monatswechsel = 1;

//  printf("Monat: %2d Variable monatswechsel: %1d\n", u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,monatswechsel);
      if ( monatswechsel == 1 )
      {
        printf("Monatswechsel!\n");
        if ( close_logfile() == -1)
        {
          printf("Fehler beim Monatswechsel: Cannot close logfile!");
          exit(-1);
        }
        else
        {
          if (uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat,u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr));
          if (uvr_typ == UVR61_3)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr));
          if ( tmp_erg == 0 )
          {
            printf("Fehler beim Monatswechsel: Der Logfile-Name konnte nicht erzeugt werden!");
            exit(-1);
          }
          else
          {
            if ( open_logfile(LogFileName, 1) == -1 )
            {
              printf("Fehler beim Monatswechsel: Das LogFile kann nicht geoeffnet werden!\n");
              exit(-1);
            }
          }
        }
      } /* Ende: if ( merk_monat != dsatz_uvr1611[0].datum_zeit.monat ) */

      if (uvr_typ == UVR1611)
        merk_monat = u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat;
      if (uvr_typ == UVR61_3)
        merk_monat = u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat;

      if (uvr_typ == UVR1611)
        copy_UVR2winsol_1611( &u_dsatz_uvr[0], &dsatz_winsol[0] );
      if (uvr_typ == UVR61_3)
        copy_UVR2winsol_61_3( &u_dsatz_uvr[0], &dsatz_winsol_uvr61_3[0] );

      if ( csv==1 && fp_csvfile != NULL )
      {
        if (uvr_typ == UVR1611)
//          writeWINSOLlogfile2CSV(fp_csvfile, &dsatz_winsol[0],
           writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat );
        if (uvr_typ == UVR61_3)
//          writeWINSOLlogfile2CSV(fp_csvfile, &dsatz_winsol[0],
          writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat );
      }

      /* puffer_dswinsol = &dsatz_winsol[0];*/
      /* schreiben der gelesenen Rohdaten in das LogFile */
      if (uvr_typ == UVR1611)
        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
      if (uvr_typ == UVR61_3)
        tmp_erg = fwrite(puffer_dswinsol_uvr61_3,59,1,fp_logfile);

      if ( ((i%100) == 0) && (i > 0) )
        printf("%d Datensaetze geschrieben.\n",i);

      if ( *end_adresse == sendbuf[1] && *(end_adresse+1) == sendbuf[2] && *(end_adresse+2) == sendbuf[3] )
	    break;
		
      /* Hochzaehlen der Startadressen                                      */
      if (lowbyte <= 2)
        lowbyte++;
      else
      {
        lowbyte = 0;
        middlebyte++;
      }

      switch (lowbyte)
      {
        case 0: sendbuf[1] = 0x00; break;
        case 1: sendbuf[1] = 0x40; break;
        case 2: sendbuf[1] = 0x80; break;
        case 3: sendbuf[1] = 0xc0; break;
      }

      if (middlebyte > merkmiddlebyte)   /* das mittlere Byte muss erhoeht werden */
      {
        if ( sendbuf[2] != 0xFE )
        {
          sendbuf[2] = sendbuf[2] + 0x02;
          merkmiddlebyte = middlebyte;
        }
        else /* das highbyte muss erhoeht werden */
        {
          sendbuf[2] = 0x00;
          sendbuf[3] = sendbuf[3] + 0x01;
          merkmiddlebyte = middlebyte;
        }
      }
	  
	  if (sendbuf[3] > 0x0F ) // "Speicherueberlauf" im BL-Net
	  {
		sendbuf[1] = 0x00;
		sendbuf[2] = 0x00;
		sendbuf[3] = 0x00;
	  }
	  
      monatswechsel = 0;
    } /* Ende: if (dsatz_uvr1611[0].pruefsum == pruefsumme) */
    else
    {
      if (merk_i < 5)
      {
        i--; /* falsche Pruefziffer - also nochmals lesen */
#ifdef DEBUG
        fprintf(stderr, " falsche Pruefsumme - Versuch#%d\n",merk_i);
#endif
        merk_i++; /* hochzaehlen bis 5 */
      }
      else
      {
        merk_i = 0;
        fehlerhafte_ds++;
        printf ( " fehlerhafter Datensatz - insgesamt:%d\n",fehlerhafte_ds);
      }
    }
  }

  return i - fehlerhafte_ds;
}

/* Daten vom DL lesen - 2DL-Modus */
int datenlesen_D1(int anz_datensaetze)
{
  unsigned modTeiler;
  int i, merk_i, fehlerhafte_ds, result=0, lowbyte, middlebyte, merkmiddlebyte, tmp_erg = 0;
  int Bytes_for_0xD1 = 127, monatswechsel = 0;
  u_modus_D1 u_dsatz_uvr[1];

  DS_Winsol dsatz_winsol[1];
  DS_Winsol *puffer_dswinsol = &dsatz_winsol[0];
  DS_Winsol dsatz_winsol_2[1];
  DS_Winsol *puffer_dswinsol_2 = &dsatz_winsol_2[0];
  DS_Winsol_UVR61_3 dsatz_winsol_uvr61_3[1];
  DS_Winsol_UVR61_3 *puffer_dswinsol_uvr61_3 = &dsatz_winsol_uvr61_3[0];
  DS_Winsol_UVR61_3 dsatz_winsol_uvr61_3_2[1];
  DS_Winsol_UVR61_3 *puffer_dswinsol_uvr61_3_2 = &dsatz_winsol_uvr61_3_2[0];
  UCHAR pruefsumme = 0, merk_monat = 0;
  UCHAR sendbuf[6];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[6];

  modTeiler = 0x100;
  i = 0; /* Gesamtdurchlaufzaehler mit 0 initialisiert */
  merk_i = 0; /* Bei falscher Pruefziffer den Datensatz bis zu fuenfmal wiederholt lesen */
  fehlerhafte_ds = 0; /* Anzahl fehlerhaft gelesener Datensaetze mit 0 initialisiert */
  lowbyte = 0;
  middlebyte = 0;
  merkmiddlebyte = middlebyte;

  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  if (usb_zugriff)
  {
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd_serialport,empfbuf,1);
  }
  if (ip_zugriff)
  {
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
    }  /* if (!ip_first) */
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
  }

  /* fuellen des Sendebuffer - 6 Byte */
  sendbuf[0] = DATENBEREICHLESEN;
  // Startadressen von Kopfsatzlesen:
  sendbuf[1] = *start_adresse;     /* Beginn des Datenbereiches (low vor high) */
  sendbuf[2] = *(start_adresse+1);
  sendbuf[3] = *(start_adresse+2);
  sendbuf[4] = 0x01;  /* Anzahl der zu lesenden Rahmen */

  for(;i<anz_datensaetze;i++)
  {
    sendbuf[5] = (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4]) % modTeiler;  /* Pruefziffer */

    if (usb_zugriff)
    {
      write_erg=write(fd_serialport,sendbuf,6);
      if (write_erg == 6)    /* Lesen der Antwort*/
        // result=read(fd_serialport, dsatz_uvr1611,65);
        result=read(fd_serialport, u_dsatz_uvr,Bytes_for_0xD1);
    }
    if (ip_zugriff)
    {
      if (!ip_first)
      {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup();
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup();
          return 3;
        }
       }  /* if (!ip_first) */

       write_erg=send(sock,sendbuf,6,0);
       if (write_erg == 6)    /* Lesen der Antwort */
          result  = recv(sock, u_dsatz_uvr,Bytes_for_0xD1,0);
        // result  = recv(sock, dsatz_uvr1611,65,0);
    } /* if (ip_zugriff) */

//**************************************************************************************** !!!!!!!!
      pruefsumme = berechnepruefziffer_modus_D1( &u_dsatz_uvr[0], result );
//      printf("Pruefsumme berechnet: %x in Byte %d erhalten %x\n",pruefsumme,result,u_dsatz_uvr[0].DS_alles.all_bytes[result-1]);
#if DEBUG > 3
  if (uvr_typ == UVR1611)
      fprintf(stderr, "%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR1611.pruefsum,pruefsumme);
  if (uvr_typ == UVR61_3)
      pfprintf(stderr, "%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR61_3.pruefsum,pruefsumme);
#endif

    if (u_dsatz_uvr[0].DS_alles.all_bytes[result-1] == pruefsumme )
    {  /*Aenderung: 02.09.06 - Hochzaehlen der Startadresse erst dann, wenn korrekt gelesen wurde (eventuell endlosschleife?) */
#if DEBUG > 4
    print_dsatz_uvr1611_content(u_dsatz_uvr);
      int zz;
      for (zz=0;zz<result-1;zz++)
        fprintf(stderr, "%2x ",u_dsatz_uvr[0].DS_alles.all_bytes[zz]);
      fprintf(stderr, "\nuvr_typ(1): 0x%x uvr_typ(2): 0x%x \n",uvr_typ,uvr_typ2);
      fprintf(stderr, "%2x \n",u_dsatz_uvr[0].DS_alles.all_bytes[59]);
#endif
      if ( i == 0 ) /* erster Datenssatz wurde gelesen - Logfile oeffnen / erstellen */
      {
        if (uvr_typ == UVR1611)
        {
          tmp_erg = erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.datum_zeit.jahr);
          merk_monat = u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat;
//          printf("merk_monat1: %d\n",merk_monat);
        }
        if (uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.jahr));
          merk_monat = u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat;
//          printf("merk_monat2: %d\n",merk_monat);
        }
//        printf("uvr_typ(1): 0x%x Gelesener Monat: %d, Jahr %d tmp_erg: %d\n",uvr_typ,merk_monat,u_dsatz_uvr[0].DS_1611_1611.datum_zeit.jahr,tmp_erg );

        if ( tmp_erg == 0 ) /* Das erste  */
        {
          printf("Der Logfile-Name (1) konnte nicht erzeugt werden!");
          exit(-1);
        }
        /* ********* 2. Geraet ********* */
        if (uvr_typ2 == UVR1611 && uvr_typ == UVR1611)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.jahr));
//        printf("Gelesener 2. Monat: %d Jahr %d\n",u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.jahr);
        }
        else if (uvr_typ2 == UVR1611 && uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.jahr));
//        printf("Gelesener 2. Monat: %d Jahr %d\n",u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.jahr);
        }
        if (uvr_typ2 == UVR61_3 && uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.jahr));
//        printf("Gelesener 2. Monat: %d Jahr %d\n",u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.jahr);
        }
        else if (uvr_typ2 == UVR61_3 && uvr_typ == UVR1611)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.jahr));
//        printf("Gelesener 2. Monat: %d Jahr %d\n",u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.jahr);
        }
        if ( tmp_erg == 0 )
        {
          printf("Der Logfile-Name (2) konnte nicht erzeugt werden!");
          exit(-1);
        }
        else
        {
          if ( open_logfile(LogFileName, 1) == -1 )
          {
            printf("Das LogFile kann nicht geoeffnet werden!\n");
            exit(-1);
          }
          if ( open_logfile(LogFileName_2, 2) == -1 )
          {
            printf("Das LogFile kann nicht geoeffnet werden!\n");
            exit(-1);
          }
        }
      }
      /* Hat der Monat gewechselt? Wenn ja, neues LogFile erstellen. */
      if ( uvr_typ == UVR1611 && merk_monat != u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat )
        monatswechsel = 1;
      if ( uvr_typ == UVR61_3 && merk_monat != u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat )
        monatswechsel = 1;

//  printf("Monat: %2d Variable monatswechsel: %1d\n", u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,monatswechsel);
      if ( monatswechsel == 1 )
      {
        printf("Monatswechsel!\n");
        if ( close_logfile() == -1)
        {
          printf("Fehler beim Monatswechsel: Cannot close logfile!");
          exit(-1);
        }
        else
        {
          if (uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.datum_zeit.jahr));
          if (uvr_typ == UVR61_3)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.jahr));

          /* ********* 2. Geraet ********* */
          if (uvr_typ2 == UVR1611 && uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.jahr));
          else if (uvr_typ2 == UVR1611 && uvr_typ == UVR61_3)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.jahr));

          if (uvr_typ2 == UVR61_3 && uvr_typ == UVR61_3)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.jahr));
          else if (uvr_typ2 == UVR61_3 && uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.jahr));

          if ( tmp_erg == 0 )
          {
            printf("Fehler beim Monatswechsel: Der Logfile-Name konnte nicht erzeugt werden!");
            exit(-1);
          }
          else
          {
            if ( open_logfile(LogFileName, 1) == -1 )
            {
              printf("Fehler beim Monatswechsel: Das LogFile kann nicht geoeffnet werden!\n");
              exit(-1);
            }
            if ( open_logfile(LogFileName_2, 2) == -1 )
            {
              printf("Das LogFile kann nicht geoeffnet werden!\n");
              exit(-1);
            }
          }
        }
      } /* Ende: if ( merk_monat != dsatz_uvr1611[0].datum_zeit.monat ) */

      if (uvr_typ == UVR1611)
        merk_monat = u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat;
      if (uvr_typ == UVR61_3)
        merk_monat = u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat;

      /* Daten in die Winsol-Struktur kopieren */
      if (uvr_typ == UVR1611)
        copy_UVR2winsol_D1_1611( &u_dsatz_uvr[0], &dsatz_winsol[0], 1 );
      if (uvr_typ == UVR61_3)
        copy_UVR2winsol_D1_61_3( &u_dsatz_uvr[0], &dsatz_winsol_uvr61_3[0], 1);
      /* schreiben der gelesenen Rohdaten in das LogFile */
      if (uvr_typ == UVR1611)
        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
      if (uvr_typ == UVR61_3)
        tmp_erg = fwrite(puffer_dswinsol_uvr61_3,59,1,fp_logfile);
//printf("Nach Schreiben der Daten 1. Geraet\n");

      /* ********* 2. Geraet ********* */
      /* Daten in die Winsol-Struktur kopieren */
      if (uvr_typ2 == UVR1611)
        copy_UVR2winsol_D1_1611( &u_dsatz_uvr[0], &dsatz_winsol_2[0], 2 );
      if (uvr_typ2 == UVR61_3)
        copy_UVR2winsol_D1_61_3( &u_dsatz_uvr[0], &dsatz_winsol_uvr61_3_2[0], 2 );
//printf("nach copy_ 2. Geraet.\n");
      /* schreiben der gelesenen Rohdaten in das LogFile */
      if (uvr_typ2 == UVR1611)
        tmp_erg = fwrite(puffer_dswinsol_2,59,1,fp_logfile_2);
      if (uvr_typ2 == UVR61_3)
        tmp_erg = fwrite(puffer_dswinsol_uvr61_3_2 ,59,1,fp_logfile_2);
//printf("Nach Schreiben der Daten 2. Geraet\n");

/*      if ( csv==1 && fp_csvfile != NULL )
      {
        if (uvr_typ == UVR1611)
//          writeWINSOLlogfile2CSV(fp_csvfile, &dsatz_winsol[0],
           writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat );
        if (uvr_typ == UVR61_3)
//          writeWINSOLlogfile2CSV(fp_csvfile, &dsatz_winsol[0],
          writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat );
      }
*/

      if ( ((i%100) == 0) && (i > 0) )
        printf("%d Datensaetze geschrieben.\n",i);

      if ( *end_adresse == sendbuf[1] && *(end_adresse+1) == sendbuf[2] && *(end_adresse+2) == sendbuf[3] )
	    break;
		
      /* Hochzaehlen der Startadressen                                      */
      if (lowbyte == 0)
        lowbyte++;
      else
      {
        lowbyte = 0;
        middlebyte++;
      }

      switch (lowbyte)
      {
        case 0: sendbuf[1] = 0x00; break;
        case 1: sendbuf[1] = 0x80; break;
      }

      if (middlebyte > merkmiddlebyte)   /* das mittlere Byte muss erhoeht werden */
      {
        if ( sendbuf[2] != 0xFE )
        {
          sendbuf[2] = sendbuf[2] + 0x02;
          merkmiddlebyte = middlebyte;
        }
        else /* das highbyte muss erhoeht werden */
        {
          sendbuf[2] = 0x00;
          sendbuf[3] = sendbuf[3] + 0x01;
          merkmiddlebyte = middlebyte;
        }
      }
	  
	  if (sendbuf[3] > 0x0F ) // "Speicherueberlauf" im BL-Net
	  {
		sendbuf[1] = 0x00;
		sendbuf[2] = 0x00;
		sendbuf[3] = 0x00;
	  }
	  
      monatswechsel = 0;
    } /* Ende: if (dsatz_uvr1611[0].pruefsum == pruefsumme) */
    else
    {
      if (merk_i < 5)
      {
        i--; /* falsche Pruefziffer - also nochmals lesen */
#ifdef DEBUG
        fprintf(stderr, " falsche Pruefsumme - Versuch#%d\n",merk_i);
#endif
        merk_i++; /* hochzaehlen bis 5 */
      }
      else
      {
        merk_i = 0;
        fehlerhafte_ds++;
        printf ( " fehlerhafter Datensatz - insgesamt:%d\n",fehlerhafte_ds);
      }
    }
  }

  return i - fehlerhafte_ds;
}

/* Daten vom DL lesen - CAN-Modus */
int datenlesen_DC(int anz_datensaetze)
{
  unsigned modTeiler;
  int i=0, y=0, merk_i=0, fehlerhafte_ds=0, result, lowbyte, middlebyte, merkmiddlebyte, tmp_erg = 0;
  int Bytes_for_0xDC = 524, monatswechsel = 0, anzahl_can_rahmen = 0;
  int pruefsum_check = 0;
  u_DS_CAN u_dsatz_can[1];
  DS_Winsol dsatz_winsol[8];  /* 8 Datensaetze moeglich */
  DS_Winsol *puffer_dswinsol = &dsatz_winsol[0];
  UCHAR pruefsumme = 0, merk_monat = 0;
  UCHAR sendbuf[6];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[18];

  modTeiler = 0x100;
  i = 0; /* Gesamtdurchlaufzaehler mit 0 initialisiert */
  merk_i = 0; /* Bei falscher Pruefziffer den Datensatz bis zu fuenfmal wiederholt lesen */
  fehlerhafte_ds = 0; /* Anzahl fehlerhaft gelesener Datensaetze mit 0 initialisiert */
  lowbyte = 0;
  middlebyte = 0;
  merkmiddlebyte = middlebyte;
  
  for(i=1;i<525;i++)
  {
    u_dsatz_can[0].all_bytes[i] = 0xFF;
  }

  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  if (usb_zugriff)
  {
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd_serialport,empfbuf,1);
	sendbuf[0]=KONFIGCAN;
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd_serialport,empfbuf,18);   /* ?? */
  }
  if (ip_zugriff)
  {
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
    }  /* if (!ip_first) */
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
	sendbuf[0]=KONFIGCAN;
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,18,0);
  }
  
  anzahl_can_rahmen = empfbuf[0];
#if DEBUG > 3
  fprintf(stderr,"Anzahl CAN-Datenrahmen: %d. \n",anzahl_can_rahmen); 
#endif

  /* fuellen des Sendebuffer - 6 Byte */
  sendbuf[0] = DATENBEREICHLESEN;
  // Startadressen von Kopfsatzlesen:
  sendbuf[1] = *start_adresse;      /* Beginn des Datenbereiches (low vor high) */
  sendbuf[2] = *(start_adresse+1);
  sendbuf[3] = *(start_adresse+2);
  sendbuf[4] = 0x01;  /* Anzahl der zu lesenden Rahmen */

  for(i=0;i<anz_datensaetze;i++)
  {
    sendbuf[5] = (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4]) % modTeiler;  /* Pruefziffer */

/* DEBUG */
fprintf(stderr," CAN-Logging-Test: %04d. Startadresse: %x %x %x\n",i+1,sendbuf[1],sendbuf[2],sendbuf[3]);

    if (usb_zugriff)
    {
      write_erg=write(fd_serialport,sendbuf,6);
      if (write_erg == 6)    /* Lesen der Antwort*/
        result=read(fd_serialport, u_dsatz_can,Bytes_for_0xDC);
    }
    if (ip_zugriff)
    {
      if (!ip_first)
      {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup();
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup();
          return 3;
        }
       }  /* if (!ip_first) */

       write_erg=send(sock,sendbuf,6,0);
       if (write_erg == 6)    /* Lesen der Antwort */
          result  = recv(sock, u_dsatz_can,Bytes_for_0xDC,0);
		  
    } /* if (ip_zugriff) */

    /* fprintf(stderr,"-> Funktionsaufruf berechnepruefziffer, Anzahl Rahmen: %d\n",anzahl_can_rahmen); */
    if (uvr_typ == UVR1611)
      pruefsumme = berechnepruefziffer_uvr1611_CAN(u_dsatz_can, anzahl_can_rahmen);
	  
    /* fprintf(stderr,"-> Funktionsende berechnepruefziffer, Pruefsumme: %d\n",pruefsumme); */
    /* fprintf(stderr,"-> uvr_typ = %x\n",uvr_typ); */

  if (uvr_typ == UVR1611)
  {
    /* fprintf(stderr,"-> Pruefsummencheck\n");  */
    switch(anzahl_can_rahmen)
	{
	  case 1: if (u_dsatz_can[0].DS_CAN_1.pruefsum == pruefsumme )
	            pruefsum_check = 1;
	        fprintf(stderr,"%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_can[0].DS_CAN_1.pruefsum,pruefsumme);
	        break;
	  case 2: if (u_dsatz_can[0].DS_CAN_2.pruefsum == pruefsumme )
	            pruefsum_check = 1;
			fprintf(stderr,"%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_can[0].DS_CAN_2.pruefsum,pruefsumme);
	        break;
	  case 3: if (u_dsatz_can[0].DS_CAN_3.pruefsum == pruefsumme )
	            pruefsum_check = 1;
            fprintf(stderr,"%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_can[0].DS_CAN_3.pruefsum,pruefsumme);
	        break;
	  case 4: if (u_dsatz_can[0].DS_CAN_4.pruefsum == pruefsumme )
	            pruefsum_check = 1;
            fprintf(stderr,"%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_can[0].DS_CAN_4.pruefsum,pruefsumme);
	        break;
	  case 5: if (u_dsatz_can[0].DS_CAN_5.pruefsum == pruefsumme )
	            pruefsum_check = 1;
            fprintf(stderr,"%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_can[0].DS_CAN_5.pruefsum,pruefsumme);
	        break;
	  case 6: if (u_dsatz_can[0].DS_CAN_6.pruefsum == pruefsumme )
	            pruefsum_check = 1;
            fprintf(stderr,"%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_can[0].DS_CAN_6.pruefsum,pruefsumme);
	        break;
	  case 7: if (u_dsatz_can[0].DS_CAN_7.pruefsum == pruefsumme )
	            pruefsum_check = 1;
            fprintf(stderr,"%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_can[0].DS_CAN_7.pruefsum,pruefsumme);
	        break;
	  case 8: if (u_dsatz_can[0].DS_CAN_8.pruefsum == pruefsumme )
	            pruefsum_check = 1;
            fprintf(stderr,"%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_can[0].DS_CAN_8.pruefsum,pruefsumme);
	        break;
	}
  }	  
  
fprintf(stderr,"-> Pruefsummencheck fertig. pruefsum_check: %i\n",pruefsum_check);  
  
    if ( pruefsum_check == 1 )
    {  /*Aenderung: 02.09.06 - Hochzaehlen der Startadresse erst dann, wenn korrekt gelesen wurde (eventuell endlosschleife?) */
//fprintf(stderr,"-> Pruefsummencheck war ok.\n");
      if ( i == 0 ) /* erster Datenssatz wurde gelesen - Logfile oeffnen / erstellen */
      {
//fprintf(stderr,"-> vor Funktionsaufruf Logfilenname erzeugen.\n");
        if (uvr_typ == UVR1611)
        {
//fprintf(stderr," --> vorbelegte Variable LogFileName_1: %s\n",LogFileName_1);
          tmp_erg = ( erzeugeLogfileName_CAN(u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat,u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.jahr,anzahl_can_rahmen) );
          merk_monat = u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat;
        }
        if ( tmp_erg == 0 )
        {
          printf("Der Logfile-Name konnte nicht erzeugt werden!");
          exit(-1);
        }
        else
        {
fprintf(stderr,"-> Funktionsaufruf Logfilenname erfolgreich.\n-> Logfile('s) oeffnen.\n");
			switch(anzahl_can_rahmen)
			{
			  case 1: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				break;
			  case 2: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Das LogFile 1 %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Das LogFile 2 %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				break;
			  case 3: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Das LogFile 1 %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Das LogFile 2 %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Das LogFile 3 %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				break;
			  case 4: 
			    if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Das LogFile 1 %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Das LogFile 2 %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Das LogFile 3 %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Das LogFile 4 %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				break;
			  case 5: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Das LogFile 1 %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Das LogFile 2 %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Das LogFile 3 %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Das LogFile 4 %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_5, 5) == -1 )
				{
				  printf("Das LogFile 5 %s kann nicht geoeffnet werden!\n",LogFileName_5);
				  exit(-1);
				}
				break;
			  case 6: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Das LogFile 1 %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Das LogFile 2 %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Das LogFile 3 %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Das LogFile 4 %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_5, 5) == -1 )
				{
				  printf("Das LogFile 5 %s kann nicht geoeffnet werden!\n",LogFileName_5);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_6, 6) == -1 )
				{
				  printf("Das LogFile 6 %s kann nicht geoeffnet werden!\n",LogFileName_6);
				  exit(-1);
				}
				break;
			  case 7: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Das LogFile 1 %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Das LogFile 2 %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Das LogFile 3 %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Das LogFile 4 %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_5, 5) == -1 )
				{
				  printf("Das LogFile 5 %s kann nicht geoeffnet werden!\n",LogFileName_5);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_6, 6) == -1 )
				{
				  printf("Das LogFile 6 %s kann nicht geoeffnet werden!\n",LogFileName_6);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_7, 7) == -1 )
				{
				  printf("Das LogFile 7 %s kann nicht geoeffnet werden!\n",LogFileName_7);
				  exit(-1);
				}
				break;
			  case 8: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Das LogFile 1 %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Das LogFile 2 %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Das LogFile 3 %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Das LogFile 4 %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_5, 5) == -1 )
				{
				  printf("Das LogFile 5 %s kann nicht geoeffnet werden!\n",LogFileName_5);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_6, 6) == -1 )
				{
				  printf("Das LogFile 6 %s kann nicht geoeffnet werden!\n",LogFileName_6);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_7, 7) == -1 )
				{
				  printf("Das LogFile 7 %s kann nicht geoeffnet werden!\n",LogFileName_7);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_8, 8) == -1 )
				{
				  printf("Das LogFile 8 %s kann nicht geoeffnet werden!\n",LogFileName_8);
				  exit(-1);
				}
			}
        }

      }
      /* Hat der Monat gewechselt? Wenn ja, neues LogFile erstellen. */
      if ( merk_monat != u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat )
        monatswechsel = 1;

//  printf("Monat: %2d Variable monatswechsel: %1d\n", u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,monatswechsel);
      if ( monatswechsel == 1 )
      {
	    switch(anzahl_can_rahmen)
		{
		  case 1: fclose(fp_logfile); break;
		  case 2: fclose(fp_logfile); fclose(fp_logfile_2); break;
		  case 3: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); break;
		  case 4: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4); break;
		  case 5: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4);
		          fclose(fp_logfile_5); break;
		  case 6: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4);
		          fclose(fp_logfile_5); fclose(fp_logfile_6); break;
		  case 7: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4);
		          fclose(fp_logfile_5); fclose(fp_logfile_6); fclose(fp_logfile_7); break;
		  case 8: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4);
		          fclose(fp_logfile_5); fclose(fp_logfile_6); fclose(fp_logfile_7); fclose(fp_logfile_8); break;
		}

        printf("Monatswechsel!\n");

		if (uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName_CAN(u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat,u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.jahr,anzahl_can_rahmen));
        if ( tmp_erg == 0 )
        {
           printf("Fehler beim Monatswechsel: Der Logfile-Name konnte nicht erzeugt werden!");
           exit(-1);
        }
        else
        {
			switch(anzahl_can_rahmen)
			{
			  case 1: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				break;
			  case 2: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				break;
			  case 3: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				break;
			  case 4: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				break;
			  case 5: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
  				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_5, 5) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_5);
				  exit(-1);
				}
				break;
			  case 6: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_5, 5) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_5);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_6, 6) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_6);
				  exit(-1);
				}
				break;
			  case 7: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_5, 5) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_5);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_6, 6) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_6);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_7, 7) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_7);
				  exit(-1);
				}
				break;
			  case 8: if ( open_logfile_CAN(LogFileName_1, 1) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_1);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_2, 2) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_2);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_3, 3) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_3);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_4, 4) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_4);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_5, 5) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_5);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_6, 6) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_6);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_7, 7) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_7);
				  exit(-1);
				}
				if ( open_logfile_CAN(LogFileName_8, 8) == -1 )
				{
				  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName_8);
				  exit(-1);
				}
			}
        }
      } /* Ende: if ( merk_monat != dsatz_uvr1611[0].datum_zeit.monat ) */
	  
      /* uvr_typ == UVR1611 */
      merk_monat = u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat;
	  
      /* uvr_typ == UVR1611 */
	  /* gelesene Datensaetze dem Winsol-Format zuordnen und in Logdateien schreiben */
	  switch(anzahl_can_rahmen)
	  {
	    case 1: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_1.DS_CAN[0], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
		        break;
	    case 2: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_2.DS_CAN[0], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
		        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_2.DS_CAN[1], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
		        break;
	    case 3: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_3.DS_CAN[0], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
		        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_3.DS_CAN[1], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_3.DS_CAN[2], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
		        break;
	    case 4: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_4.DS_CAN[0], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
		        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_4.DS_CAN[1], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_4.DS_CAN[2], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_4.DS_CAN[3], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
		        break;
	    case 5: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[0], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
		        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[1], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[2], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[3], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[4], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_5);
		        break;
	    case 6: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[0], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
		        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[1], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[2], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[3], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[4], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_5);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[5], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_6);
		        break;
	    case 7: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[0], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
		        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[1], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[2], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[3], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[4], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_5);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[5], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_6);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[6], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_7);
		        break;
	    case 8: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[0], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
		        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[1], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[2], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[3], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[4], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_5);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[5], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_6);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[6], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_7);
				copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[7], &dsatz_winsol[0] );
		        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_8);
		        break;
	  }
      
fprintf(stderr,"-> %d. Datensatz geschrieben.\n",i);

/*
      if ( csv==1 && fp_csvfile != NULL )
      {
        if (uvr_typ == UVR1611)
           writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat );
      }
*/

      if ( ((i%100) == 0) && (i > 0) )
        printf("%d Datensaetze geschrieben.\n",i);

      if ( *end_adresse == sendbuf[1] && *(end_adresse+1) == sendbuf[2] && *(end_adresse+2) == sendbuf[3] )
	    break;
		
      /* Hochzaehlen der Startadressen                                      */
      if (lowbyte <= 2)
        lowbyte++;
      else
      {
        lowbyte = 0;
      }

	  switch (anzahl_can_rahmen)
	  {
	    case 1: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; middlebyte++; break;
                   case 1: sendbuf[1] = 0x40; break;
                   case 2: sendbuf[1] = 0x80; break;
                   case 3: sendbuf[1] = 0xc0; break;
                }
				if (middlebyte > merkmiddlebyte)   /* das mittlere Byte muss erhoeht werden */
				{
					sendbuf[2] = sendbuf[2] + 0x02;
					merkmiddlebyte = middlebyte;
				}
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
				{
					sendbuf[2] = 0x00;
					sendbuf[3] = sendbuf[3] + 0x01;
					merkmiddlebyte = middlebyte;
				}
				break;
		case 2: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; middlebyte++; break;
                   case 1: sendbuf[1] = 0x80; lowbyte = 3; break;
                }
				if (middlebyte > merkmiddlebyte)   /* das mittlere Byte muss erhoeht werden */
				{
					sendbuf[2] = sendbuf[2] + 0x02;
					merkmiddlebyte = middlebyte;
				}
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
				{
					sendbuf[2] = 0x00;
					sendbuf[3] = sendbuf[3] + 0x01;
					merkmiddlebyte = middlebyte;
				}
				break;
	    case 3: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; break;
                   case 1: sendbuf[1] = 0xc0; break;
                   case 2: sendbuf[1] = 0x80; break;
                   case 3: sendbuf[1] = 0x40; break;
                }
				if ( y == 0 )
				    y++;
				else
				{
				    sendbuf[2] = sendbuf[2] + 0x02;
					y++;
				}
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
				break;
		case 4: sendbuf[1] = 0x00;
                sendbuf[2] = sendbuf[2] + 0x02;
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
				break;
	    case 5: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; break;
                   case 1: sendbuf[1] = 0x40; break;
                   case 2: sendbuf[1] = 0x80; break;
                   case 3: sendbuf[1] = 0xc0; break;
                }
			    if ( y == 3 )
			    {
			      sendbuf[2] = sendbuf[2] + 0x04;
			      y++;
			    }
			    else
			    {
			      sendbuf[2] = sendbuf[2] + 0x02;
				  y++;
			    }
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                   sendbuf[2] = 0x00;
                   sendbuf[3] = sendbuf[3] + 0x01;
                }
				break;
		case 6: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00;
                           sendbuf[2] = sendbuf[2] + 0x04;
           				break;
                   case 1: sendbuf[1] = 0x80;
                           sendbuf[2] = sendbuf[2] + 0x02;
						   lowbyte = 3;
           				break;
                }
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
				break;
	    case 7: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; break;
                   case 1: sendbuf[1] = 0xc0; break;
                   case 2: sendbuf[1] = 0x80; break;
                   case 3: sendbuf[1] = 0x40; break;
                }
    		    if ( y == 0 )
				{
				    sendbuf[2] = sendbuf[2] + 0x02;
				    y++;
				}
				else
				{
				    sendbuf[2] = sendbuf[2] + 0x04;
				    y++;
				}
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
				break;
		case 8: sendbuf[1] = 0x00;
                sendbuf[2] = sendbuf[2] + 0x04;
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
				break;
	  }

      if ( y == 4 )
	    y = 0;
		
	  if (sendbuf[3] > 0x0F ) // "Speicherueberlauf" im BL-Net
	  {
		sendbuf[1] = 0x00;
		sendbuf[2] = 0x00;
		sendbuf[3] = 0x00;
	  }
		
      monatswechsel = 0;
    } /* Ende: if (dsatz_uvr1611[0].pruefsum == pruefsumme) */
    else
    {
      if (merk_i < 5)
      {
        i--; /* falsche Pruefziffer - also nochmals lesen */
#ifdef DEBUG
        fprintf(stderr, " falsche Pruefsumme - Versuch#%d\n",merk_i);
#endif
        merk_i++; /* hochzaehlen bis 5 */
      }
      else
      {
        merk_i = 0;
        fehlerhafte_ds++;
        printf ( " fehlerhafter Datensatz - insgesamt:%d\n",fehlerhafte_ds);
      }
    }
  }

  return i - fehlerhafte_ds;
}


/* Berechnung der Pruefsumme des Kopfsatz Modus 0xD1 */
int berechneKopfpruefziffer_D1(KopfsatzD1 derKopf[] )
{
  return  ((derKopf[0].kennung + derKopf[0].version
     + derKopf[0].zeitstempel[0]
     + derKopf[0].zeitstempel[1]
     + derKopf[0].zeitstempel[2]
     + derKopf[0].satzlaengeGeraet1
     + derKopf[0].satzlaengeGeraet2
     + derKopf[0].startadresse[0]
     + derKopf[0].startadresse[1]
     + derKopf[0].startadresse[2]
     + derKopf[0].endadresse[0]
     + derKopf[0].endadresse[1]
     + derKopf[0].endadresse[2]) % 0x100);
}

/* Berechnung der Pruefsumme des Kopfsatz Modus 0xDC */
int berechneKopfpruefziffer_DC(KOPFSATZ_DC derKopf[] )
{
  int retval=0;
  
   switch(derKopf[0].all_bytes[5])
  {
    case 1:
    retval = ((derKopf[0].DC_Rahmen1.kennung + derKopf[0].DC_Rahmen1.version
     + derKopf[0].DC_Rahmen1.zeitstempel[0]
     + derKopf[0].DC_Rahmen1.zeitstempel[1]
     + derKopf[0].DC_Rahmen1.zeitstempel[2]
     + derKopf[0].DC_Rahmen1.anzahlCAN_Rahmen
     + derKopf[0].DC_Rahmen1.satzlaengeRahmen1
     + derKopf[0].DC_Rahmen1.startadresse[0]
     + derKopf[0].DC_Rahmen1.startadresse[1]
     + derKopf[0].DC_Rahmen1.startadresse[2]
     + derKopf[0].DC_Rahmen1.endadresse[0]
     + derKopf[0].DC_Rahmen1.endadresse[1]
     + derKopf[0].DC_Rahmen1.endadresse[2]) % 0x100);
      break;
	case 2:
    retval = ((derKopf[0].DC_Rahmen2.kennung + derKopf[0].DC_Rahmen2.version
     + derKopf[0].DC_Rahmen2.zeitstempel[0]
     + derKopf[0].DC_Rahmen2.zeitstempel[1]
     + derKopf[0].DC_Rahmen2.zeitstempel[2]
     + derKopf[0].DC_Rahmen2.anzahlCAN_Rahmen
     + derKopf[0].DC_Rahmen2.satzlaengeRahmen1
     + derKopf[0].DC_Rahmen2.satzlaengeRahmen2
     + derKopf[0].DC_Rahmen2.startadresse[0]
     + derKopf[0].DC_Rahmen2.startadresse[1]
     + derKopf[0].DC_Rahmen2.startadresse[2]
     + derKopf[0].DC_Rahmen2.endadresse[0]
     + derKopf[0].DC_Rahmen2.endadresse[1]
     + derKopf[0].DC_Rahmen2.endadresse[2]) % 0x100);
      break;
	case 3:
    retval = ((derKopf[0].DC_Rahmen3.kennung + derKopf[0].DC_Rahmen3.version
     + derKopf[0].DC_Rahmen3.zeitstempel[0]
     + derKopf[0].DC_Rahmen3.zeitstempel[1]
     + derKopf[0].DC_Rahmen3.zeitstempel[2]
     + derKopf[0].DC_Rahmen3.anzahlCAN_Rahmen
     + derKopf[0].DC_Rahmen3.satzlaengeRahmen1
     + derKopf[0].DC_Rahmen3.satzlaengeRahmen2
     + derKopf[0].DC_Rahmen3.satzlaengeRahmen3
     + derKopf[0].DC_Rahmen3.startadresse[0]
     + derKopf[0].DC_Rahmen3.startadresse[1]
     + derKopf[0].DC_Rahmen3.startadresse[2]
     + derKopf[0].DC_Rahmen3.endadresse[0]
     + derKopf[0].DC_Rahmen3.endadresse[1]
     + derKopf[0].DC_Rahmen3.endadresse[2]) % 0x100);
      break;
	case 4:
    retval = ((derKopf[0].DC_Rahmen4.kennung + derKopf[0].DC_Rahmen4.version
     + derKopf[0].DC_Rahmen4.zeitstempel[0]
     + derKopf[0].DC_Rahmen4.zeitstempel[1]
     + derKopf[0].DC_Rahmen4.zeitstempel[2]
     + derKopf[0].DC_Rahmen4.anzahlCAN_Rahmen
     + derKopf[0].DC_Rahmen4.satzlaengeRahmen1
     + derKopf[0].DC_Rahmen4.satzlaengeRahmen2
     + derKopf[0].DC_Rahmen4.satzlaengeRahmen3
     + derKopf[0].DC_Rahmen4.satzlaengeRahmen4
     + derKopf[0].DC_Rahmen4.startadresse[0]
     + derKopf[0].DC_Rahmen4.startadresse[1]
     + derKopf[0].DC_Rahmen4.startadresse[2]
     + derKopf[0].DC_Rahmen4.endadresse[0]
     + derKopf[0].DC_Rahmen4.endadresse[1]
     + derKopf[0].DC_Rahmen4.endadresse[2]) % 0x100);
      break;
	case 5:
    retval = ((derKopf[0].DC_Rahmen5.kennung + derKopf[0].DC_Rahmen5.version
     + derKopf[0].DC_Rahmen5.zeitstempel[0]
     + derKopf[0].DC_Rahmen5.zeitstempel[1]
     + derKopf[0].DC_Rahmen5.zeitstempel[2]
     + derKopf[0].DC_Rahmen5.anzahlCAN_Rahmen
     + derKopf[0].DC_Rahmen5.satzlaengeRahmen1
     + derKopf[0].DC_Rahmen5.satzlaengeRahmen2
     + derKopf[0].DC_Rahmen5.satzlaengeRahmen3
     + derKopf[0].DC_Rahmen5.satzlaengeRahmen4
     + derKopf[0].DC_Rahmen5.satzlaengeRahmen5
     + derKopf[0].DC_Rahmen5.startadresse[0]
     + derKopf[0].DC_Rahmen5.startadresse[1]
     + derKopf[0].DC_Rahmen5.startadresse[2]
     + derKopf[0].DC_Rahmen5.endadresse[0]
     + derKopf[0].DC_Rahmen5.endadresse[1]
     + derKopf[0].DC_Rahmen5.endadresse[2]) % 0x100);
      break;
	case 6:
    retval = ((derKopf[0].DC_Rahmen6.kennung + derKopf[0].DC_Rahmen6.version
     + derKopf[0].DC_Rahmen6.zeitstempel[0]
     + derKopf[0].DC_Rahmen6.zeitstempel[1]
     + derKopf[0].DC_Rahmen6.zeitstempel[2]
     + derKopf[0].DC_Rahmen6.anzahlCAN_Rahmen
     + derKopf[0].DC_Rahmen6.satzlaengeRahmen1
     + derKopf[0].DC_Rahmen6.satzlaengeRahmen2
     + derKopf[0].DC_Rahmen6.satzlaengeRahmen3
     + derKopf[0].DC_Rahmen6.satzlaengeRahmen4
     + derKopf[0].DC_Rahmen6.satzlaengeRahmen5
     + derKopf[0].DC_Rahmen6.satzlaengeRahmen6
     + derKopf[0].DC_Rahmen6.startadresse[0]
     + derKopf[0].DC_Rahmen6.startadresse[1]
     + derKopf[0].DC_Rahmen6.startadresse[2]
     + derKopf[0].DC_Rahmen6.endadresse[0]
     + derKopf[0].DC_Rahmen6.endadresse[1]
     + derKopf[0].DC_Rahmen6.endadresse[2]) % 0x100);
      break;
	case 7:
    retval = ((derKopf[0].DC_Rahmen7.kennung + derKopf[0].DC_Rahmen7.version
     + derKopf[0].DC_Rahmen7.zeitstempel[0]
     + derKopf[0].DC_Rahmen7.zeitstempel[1]
     + derKopf[0].DC_Rahmen7.zeitstempel[2]
     + derKopf[0].DC_Rahmen7.anzahlCAN_Rahmen
     + derKopf[0].DC_Rahmen7.satzlaengeRahmen1
     + derKopf[0].DC_Rahmen7.satzlaengeRahmen2
     + derKopf[0].DC_Rahmen7.satzlaengeRahmen3
     + derKopf[0].DC_Rahmen7.satzlaengeRahmen4
     + derKopf[0].DC_Rahmen7.satzlaengeRahmen5
     + derKopf[0].DC_Rahmen7.satzlaengeRahmen6
     + derKopf[0].DC_Rahmen7.satzlaengeRahmen7
     + derKopf[0].DC_Rahmen7.startadresse[0]
     + derKopf[0].DC_Rahmen7.startadresse[1]
     + derKopf[0].DC_Rahmen7.startadresse[2]
     + derKopf[0].DC_Rahmen7.endadresse[0]
     + derKopf[0].DC_Rahmen7.endadresse[1]
     + derKopf[0].DC_Rahmen7.endadresse[2]) % 0x100);
      break;
	case 8:
    retval = ((derKopf[0].DC_Rahmen8.kennung + derKopf[0].DC_Rahmen8.version
     + derKopf[0].DC_Rahmen8.zeitstempel[0]
     + derKopf[0].DC_Rahmen8.zeitstempel[1]
     + derKopf[0].DC_Rahmen8.zeitstempel[2]
     + derKopf[0].DC_Rahmen8.anzahlCAN_Rahmen
     + derKopf[0].DC_Rahmen8.satzlaengeRahmen1
     + derKopf[0].DC_Rahmen8.satzlaengeRahmen2
     + derKopf[0].DC_Rahmen8.satzlaengeRahmen3
     + derKopf[0].DC_Rahmen8.satzlaengeRahmen4
     + derKopf[0].DC_Rahmen8.satzlaengeRahmen5
     + derKopf[0].DC_Rahmen8.satzlaengeRahmen6
     + derKopf[0].DC_Rahmen8.satzlaengeRahmen7
     + derKopf[0].DC_Rahmen8.satzlaengeRahmen8
     + derKopf[0].DC_Rahmen8.startadresse[0]
     + derKopf[0].DC_Rahmen8.startadresse[1]
     + derKopf[0].DC_Rahmen8.startadresse[2]
     + derKopf[0].DC_Rahmen8.endadresse[0]
     + derKopf[0].DC_Rahmen8.endadresse[1]
     + derKopf[0].DC_Rahmen8.endadresse[2]) % 0x100);
      break;
  } 
  return retval;
}

/* Berechnung der Pruefsumme des Kopfsatz Modus 0xA8 */
int berechneKopfpruefziffer_A8(KopfsatzA8 derKopf[] )
{
  return  ((derKopf[0].kennung + derKopf[0].version
     + derKopf[0].zeitstempel[0]
     + derKopf[0].zeitstempel[1]
     + derKopf[0].zeitstempel[2]
     + derKopf[0].satzlaengeGeraet1
     + derKopf[0].startadresse[0]
     + derKopf[0].startadresse[1]
     + derKopf[0].startadresse[2]
     + derKopf[0].endadresse[0]
     + derKopf[0].endadresse[1]
     + derKopf[0].endadresse[2]) % 0x100);
}

/* Berechnung der Pruefsumme fuer UVR1611 */
int berechnepruefziffer_uvr1611(u_DS_UVR1611_UVR61_3 ds_uvr1611[])
{
  unsigned modTeiler;
  modTeiler = 0x100;    /* modTeiler = 256; */

  int retval=0;

  int k=0;
  for (k=0;k<16;k++)
    retval+= (ds_uvr1611[0].DS_UVR1611.sensT[k][0]+ds_uvr1611[0].DS_UVR1611.sensT[k][1]);

  retval += ds_uvr1611[0].DS_UVR1611.ausgbyte1+ds_uvr1611[0].DS_UVR1611.ausgbyte2+
    ds_uvr1611[0].DS_UVR1611.dza[0]+ds_uvr1611[0].DS_UVR1611.dza[1]+ds_uvr1611[0].DS_UVR1611.dza[2]+ds_uvr1611[0].DS_UVR1611.dza[3]+
    ds_uvr1611[0].DS_UVR1611.wmzaehler_reg+
    ds_uvr1611[0].DS_UVR1611.mlstg1[0]+ds_uvr1611[0].DS_UVR1611.mlstg1[1]+
    ds_uvr1611[0].DS_UVR1611.mlstg1[2]+ds_uvr1611[0].DS_UVR1611.mlstg1[3]+
    ds_uvr1611[0].DS_UVR1611.kwh1[0]+ds_uvr1611[0].DS_UVR1611.kwh1[1]+
    ds_uvr1611[0].DS_UVR1611.mwh1[0]+ds_uvr1611[0].DS_UVR1611.mwh1[1]+
    ds_uvr1611[0].DS_UVR1611.mlstg2[0]+ds_uvr1611[0].DS_UVR1611.mlstg2[1]+
    ds_uvr1611[0].DS_UVR1611.mlstg2[2]+ds_uvr1611[0].DS_UVR1611.mlstg2[3]+
    ds_uvr1611[0].DS_UVR1611.kwh2[0]+ds_uvr1611[0].DS_UVR1611.kwh2[1]+
    ds_uvr1611[0].DS_UVR1611.mwh2[0]+ds_uvr1611[0].DS_UVR1611.mwh2[1]+
    ds_uvr1611[0].DS_UVR1611.datum_zeit.sek+ds_uvr1611[0].DS_UVR1611.datum_zeit.min+
    ds_uvr1611[0].DS_UVR1611.datum_zeit.std+
    ds_uvr1611[0].DS_UVR1611.datum_zeit.tag+ds_uvr1611[0].DS_UVR1611.datum_zeit.monat+
    ds_uvr1611[0].DS_UVR1611.datum_zeit.jahr+
    ds_uvr1611[0].DS_UVR1611.zeitstempel[0]+ds_uvr1611[0].DS_UVR1611.zeitstempel[1]+
    ds_uvr1611[0].DS_UVR1611.zeitstempel[2];

  return retval % modTeiler;
}

/* Berechnung der Pruefsumme fuer UVR1611 CAN-Logging */
int berechnepruefziffer_uvr1611_CAN(u_DS_CAN dsatz_can[], int anzahl_can_rahmen)
{
  unsigned modTeiler = 0x100;    /* modTeiler = 256; */
  int retval=0, k = 0, i;

  switch(anzahl_can_rahmen)
  {
    case 1:
           for (k=0;k<16;k++)
             retval+= (dsatz_can[0].DS_CAN_1.DS_CAN[0].sensT[k][0]+dsatz_can[0].DS_CAN_1.DS_CAN[0].sensT[k][1]);
           retval += dsatz_can[0].DS_CAN_1.DS_CAN[0].ausgbyte1+dsatz_can[0].DS_CAN_1.DS_CAN[0].ausgbyte2+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].dza[0]+dsatz_can[0].DS_CAN_1.DS_CAN[0].dza[1]+dsatz_can[0].DS_CAN_1.DS_CAN[0].dza[2]+dsatz_can[0].DS_CAN_1.DS_CAN[0].dza[3]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].wmzaehler_reg+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].mlstg1[0]+dsatz_can[0].DS_CAN_1.DS_CAN[0].mlstg1[1]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].mlstg1[2]+dsatz_can[0].DS_CAN_1.DS_CAN[0].mlstg1[3]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].kwh1[0]+dsatz_can[0].DS_CAN_1.DS_CAN[0].kwh1[1]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].mwh1[0]+dsatz_can[0].DS_CAN_1.DS_CAN[0].mwh1[1]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].mlstg2[0]+dsatz_can[0].DS_CAN_1.DS_CAN[0].mlstg2[1]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].mlstg2[2]+dsatz_can[0].DS_CAN_1.DS_CAN[0].mlstg2[3]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].kwh2[0]+dsatz_can[0].DS_CAN_1.DS_CAN[0].kwh2[1]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].mwh2[0]+dsatz_can[0].DS_CAN_1.DS_CAN[0].mwh2[1]+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.sek+dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.min+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.std+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.tag+dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat+
           dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.jahr+
           dsatz_can[0].DS_CAN_1.zeitstempel[0]+dsatz_can[0].DS_CAN_1.zeitstempel[1]+
           dsatz_can[0].DS_CAN_1.zeitstempel[2];
           return retval % modTeiler;
	case 2:
	       for (i=0;i<2;i++)
		   {
             for (k=0;k<16;k++)
               retval+= (dsatz_can[0].DS_CAN_2.DS_CAN[i].sensT[k][0] + dsatz_can[0].DS_CAN_2.DS_CAN[i].sensT[k][1]);
             retval += dsatz_can[0].DS_CAN_2.DS_CAN[i].ausgbyte1 + dsatz_can[0].DS_CAN_2.DS_CAN[i].ausgbyte2 +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].dza[0] + dsatz_can[0].DS_CAN_2.DS_CAN[i].dza[1] + dsatz_can[0].DS_CAN_2.DS_CAN[i].dza[2] + dsatz_can[0].DS_CAN_2.DS_CAN[i].dza[3] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].wmzaehler_reg +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].mlstg1[0] + dsatz_can[0].DS_CAN_2.DS_CAN[i].mlstg1[1] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].mlstg1[2] + dsatz_can[0].DS_CAN_2.DS_CAN[i].mlstg1[3] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].kwh1[0] + dsatz_can[0].DS_CAN_2.DS_CAN[i].kwh1[1] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].mwh1[0] + dsatz_can[0].DS_CAN_2.DS_CAN[i].mwh1[1] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].mlstg2[0] + dsatz_can[0].DS_CAN_2.DS_CAN[i].mlstg2[1] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].mlstg2[2] + dsatz_can[0].DS_CAN_2.DS_CAN[i].mlstg2[3] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].kwh2[0] + dsatz_can[0].DS_CAN_2.DS_CAN[i].kwh2[1] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].mwh2[0] + dsatz_can[0].DS_CAN_2.DS_CAN[i].mwh2[1] +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].datum_zeit.sek + dsatz_can[0].DS_CAN_2.DS_CAN[i].datum_zeit.min +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].datum_zeit.std +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].datum_zeit.tag+dsatz_can[0].DS_CAN_2.DS_CAN[i].datum_zeit.monat +
             dsatz_can[0].DS_CAN_2.DS_CAN[i].datum_zeit.jahr;
		   }
             retval += dsatz_can[0].DS_CAN_2.zeitstempel[0] + dsatz_can[0].DS_CAN_2.zeitstempel[1] + dsatz_can[0].DS_CAN_2.zeitstempel[2];
           return retval % modTeiler;
	case 3:
	       for (i=0;i<3;i++)
		   {
             for (k=0;k<16;k++)
               retval+= (dsatz_can[0].DS_CAN_3.DS_CAN[i].sensT[k][0]+dsatz_can[0].DS_CAN_3.DS_CAN[i].sensT[k][1]);
             retval += dsatz_can[0].DS_CAN_3.DS_CAN[i].ausgbyte1+dsatz_can[0].DS_CAN_3.DS_CAN[i].ausgbyte2+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].dza[0]+dsatz_can[0].DS_CAN_3.DS_CAN[i].dza[1]+dsatz_can[0].DS_CAN_3.DS_CAN[i].dza[2]+dsatz_can[0].DS_CAN_3.DS_CAN[i].dza[3]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].wmzaehler_reg+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].mlstg1[0]+dsatz_can[0].DS_CAN_3.DS_CAN[i].mlstg1[1]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].mlstg1[2]+dsatz_can[0].DS_CAN_3.DS_CAN[i].mlstg1[3]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].kwh1[0]+dsatz_can[0].DS_CAN_3.DS_CAN[i].kwh1[1]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].mwh1[0]+dsatz_can[0].DS_CAN_3.DS_CAN[i].mwh1[1]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].mlstg2[0]+dsatz_can[0].DS_CAN_3.DS_CAN[i].mlstg2[1]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].mlstg2[2]+dsatz_can[0].DS_CAN_3.DS_CAN[i].mlstg2[3]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].kwh2[0]+dsatz_can[0].DS_CAN_3.DS_CAN[i].kwh2[1]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].mwh2[0]+dsatz_can[0].DS_CAN_3.DS_CAN[i].mwh2[1]+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].datum_zeit.sek+dsatz_can[0].DS_CAN_3.DS_CAN[i].datum_zeit.min+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].datum_zeit.std+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].datum_zeit.tag+dsatz_can[0].DS_CAN_3.DS_CAN[i].datum_zeit.monat+
             dsatz_can[0].DS_CAN_3.DS_CAN[i].datum_zeit.jahr;
		   }
		   retval += dsatz_can[0].DS_CAN_3.zeitstempel[0]+dsatz_can[0].DS_CAN_3.zeitstempel[1]+dsatz_can[0].DS_CAN_3.zeitstempel[2];
           return retval % modTeiler;
	case 4:
	       for (i=0;i<4;i++)
		   {
             for (k=0;k<16;k++)
               retval+= (dsatz_can[0].DS_CAN_4.DS_CAN[i].sensT[k][0]+dsatz_can[0].DS_CAN_4.DS_CAN[i].sensT[k][1]);
             retval += dsatz_can[0].DS_CAN_4.DS_CAN[i].ausgbyte1+dsatz_can[0].DS_CAN_4.DS_CAN[i].ausgbyte2+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].dza[0]+dsatz_can[0].DS_CAN_4.DS_CAN[i].dza[1]+dsatz_can[0].DS_CAN_4.DS_CAN[i].dza[2]+dsatz_can[0].DS_CAN_4.DS_CAN[i].dza[3]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].wmzaehler_reg+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].mlstg1[0]+dsatz_can[0].DS_CAN_4.DS_CAN[i].mlstg1[1]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].mlstg1[2]+dsatz_can[0].DS_CAN_4.DS_CAN[i].mlstg1[3]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].kwh1[0]+dsatz_can[0].DS_CAN_4.DS_CAN[i].kwh1[1]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].mwh1[0]+dsatz_can[0].DS_CAN_4.DS_CAN[i].mwh1[1]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].mlstg2[0]+dsatz_can[0].DS_CAN_4.DS_CAN[i].mlstg2[1]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].mlstg2[2]+dsatz_can[0].DS_CAN_4.DS_CAN[i].mlstg2[3]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].kwh2[0]+dsatz_can[0].DS_CAN_4.DS_CAN[i].kwh2[1]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].mwh2[0]+dsatz_can[0].DS_CAN_4.DS_CAN[i].mwh2[1]+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].datum_zeit.sek+dsatz_can[0].DS_CAN_4.DS_CAN[i].datum_zeit.min+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].datum_zeit.std+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].datum_zeit.tag+dsatz_can[0].DS_CAN_4.DS_CAN[i].datum_zeit.monat+
             dsatz_can[0].DS_CAN_4.DS_CAN[i].datum_zeit.jahr;
		   } 
		   retval += dsatz_can[0].DS_CAN_4.zeitstempel[0]+dsatz_can[0].DS_CAN_4.zeitstempel[1]+dsatz_can[0].DS_CAN_4.zeitstempel[2];
           return retval % modTeiler;
	case 5:
	       for (i=0;i<5;i++)
		   {
             for (k=0;k<16;k++)
               retval+= (dsatz_can[0].DS_CAN_5.DS_CAN[i].sensT[k][0]+dsatz_can[0].DS_CAN_5.DS_CAN[i].sensT[k][1]);
             retval += dsatz_can[0].DS_CAN_5.DS_CAN[i].ausgbyte1+dsatz_can[0].DS_CAN_5.DS_CAN[i].ausgbyte2+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].dza[0]+dsatz_can[0].DS_CAN_5.DS_CAN[i].dza[1]+dsatz_can[0].DS_CAN_5.DS_CAN[i].dza[2]+dsatz_can[0].DS_CAN_5.DS_CAN[i].dza[3]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].wmzaehler_reg+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].mlstg1[0]+dsatz_can[0].DS_CAN_5.DS_CAN[i].mlstg1[1]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].mlstg1[2]+dsatz_can[0].DS_CAN_5.DS_CAN[i].mlstg1[3]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].kwh1[0]+dsatz_can[0].DS_CAN_5.DS_CAN[i].kwh1[1]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].mwh1[0]+dsatz_can[0].DS_CAN_5.DS_CAN[i].mwh1[1]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].mlstg2[0]+dsatz_can[0].DS_CAN_5.DS_CAN[i].mlstg2[1]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].mlstg2[2]+dsatz_can[0].DS_CAN_5.DS_CAN[i].mlstg2[3]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].kwh2[0]+dsatz_can[0].DS_CAN_5.DS_CAN[i].kwh2[1]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].mwh2[0]+dsatz_can[0].DS_CAN_5.DS_CAN[i].mwh2[1]+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].datum_zeit.sek+dsatz_can[0].DS_CAN_5.DS_CAN[i].datum_zeit.min+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].datum_zeit.std+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].datum_zeit.tag+dsatz_can[0].DS_CAN_5.DS_CAN[i].datum_zeit.monat+
             dsatz_can[0].DS_CAN_5.DS_CAN[i].datum_zeit.jahr;
            }
		   retval += dsatz_can[0].DS_CAN_5.zeitstempel[0]+dsatz_can[0].DS_CAN_5.zeitstempel[1]+dsatz_can[0].DS_CAN_5.zeitstempel[2];
           return retval % modTeiler;
	case 6:
	       for (i=0;i<6;i++)
		   {
             for (k=0;k<16;k++)
               retval+= (dsatz_can[0].DS_CAN_6.DS_CAN[i].sensT[k][0]+dsatz_can[0].DS_CAN_6.DS_CAN[i].sensT[k][1]);
             retval += dsatz_can[0].DS_CAN_6.DS_CAN[i].ausgbyte1+dsatz_can[0].DS_CAN_6.DS_CAN[i].ausgbyte2+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].dza[0]+dsatz_can[0].DS_CAN_6.DS_CAN[i].dza[1]+dsatz_can[0].DS_CAN_6.DS_CAN[i].dza[2]+dsatz_can[0].DS_CAN_6.DS_CAN[i].dza[3]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].wmzaehler_reg+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].mlstg1[0]+dsatz_can[0].DS_CAN_6.DS_CAN[i].mlstg1[1]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].mlstg1[2]+dsatz_can[0].DS_CAN_6.DS_CAN[i].mlstg1[3]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].kwh1[0]+dsatz_can[0].DS_CAN_6.DS_CAN[i].kwh1[1]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].mwh1[0]+dsatz_can[0].DS_CAN_6.DS_CAN[i].mwh1[1]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].mlstg2[0]+dsatz_can[0].DS_CAN_6.DS_CAN[i].mlstg2[1]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].mlstg2[2]+dsatz_can[0].DS_CAN_6.DS_CAN[i].mlstg2[3]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].kwh2[0]+dsatz_can[0].DS_CAN_6.DS_CAN[i].kwh2[1]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].mwh2[0]+dsatz_can[0].DS_CAN_6.DS_CAN[i].mwh2[1]+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].datum_zeit.sek+dsatz_can[0].DS_CAN_6.DS_CAN[i].datum_zeit.min+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].datum_zeit.std+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].datum_zeit.tag+dsatz_can[0].DS_CAN_6.DS_CAN[i].datum_zeit.monat+
             dsatz_can[0].DS_CAN_6.DS_CAN[i].datum_zeit.jahr;
		   }
		   retval += dsatz_can[0].DS_CAN_6.zeitstempel[0]+dsatz_can[0].DS_CAN_6.zeitstempel[1]+dsatz_can[0].DS_CAN_6.zeitstempel[2];
           return retval % modTeiler;
	case 7:
	       for (i=0;i<7;i++)
		   {
             for (k=0;k<16;k++)
               retval+= (dsatz_can[0].DS_CAN_7.DS_CAN[i].sensT[k][0]+dsatz_can[0].DS_CAN_7.DS_CAN[i].sensT[k][1]);
             retval += dsatz_can[0].DS_CAN_7.DS_CAN[i].ausgbyte1+dsatz_can[0].DS_CAN_7.DS_CAN[i].ausgbyte2+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].dza[0]+dsatz_can[0].DS_CAN_7.DS_CAN[i].dza[1]+dsatz_can[0].DS_CAN_7.DS_CAN[i].dza[2]+dsatz_can[0].DS_CAN_7.DS_CAN[i].dza[3]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].wmzaehler_reg+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].mlstg1[0]+dsatz_can[0].DS_CAN_7.DS_CAN[i].mlstg1[1]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].mlstg1[2]+dsatz_can[0].DS_CAN_7.DS_CAN[i].mlstg1[3]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].kwh1[0]+dsatz_can[0].DS_CAN_7.DS_CAN[i].kwh1[1]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].mwh1[0]+dsatz_can[0].DS_CAN_7.DS_CAN[i].mwh1[1]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].mlstg2[0]+dsatz_can[0].DS_CAN_7.DS_CAN[i].mlstg2[1]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].mlstg2[2]+dsatz_can[0].DS_CAN_7.DS_CAN[i].mlstg2[3]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].kwh2[0]+dsatz_can[0].DS_CAN_7.DS_CAN[i].kwh2[1]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].mwh2[0]+dsatz_can[0].DS_CAN_7.DS_CAN[i].mwh2[1]+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].datum_zeit.sek+dsatz_can[0].DS_CAN_7.DS_CAN[i].datum_zeit.min+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].datum_zeit.std+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].datum_zeit.tag+dsatz_can[0].DS_CAN_7.DS_CAN[i].datum_zeit.monat+
             dsatz_can[0].DS_CAN_7.DS_CAN[i].datum_zeit.jahr;
		   }
		   retval += dsatz_can[0].DS_CAN_7.zeitstempel[0]+dsatz_can[0].DS_CAN_7.zeitstempel[1]+dsatz_can[0].DS_CAN_7.zeitstempel[2];
           return retval % modTeiler;
	case 8:
	       for (i=0;i<8;i++)
		   {
             for (k=0;k<16;k++)
               retval+= (dsatz_can[0].DS_CAN_8.DS_CAN[i].sensT[k][0]+dsatz_can[0].DS_CAN_8.DS_CAN[i].sensT[k][1]);
             retval += dsatz_can[0].DS_CAN_8.DS_CAN[i].ausgbyte1+dsatz_can[0].DS_CAN_8.DS_CAN[i].ausgbyte2+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].dza[0]+dsatz_can[0].DS_CAN_8.DS_CAN[i].dza[1]+dsatz_can[0].DS_CAN_8.DS_CAN[i].dza[2]+dsatz_can[0].DS_CAN_8.DS_CAN[i].dza[3]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].wmzaehler_reg+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].mlstg1[0]+dsatz_can[0].DS_CAN_8.DS_CAN[i].mlstg1[1]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].mlstg1[2]+dsatz_can[0].DS_CAN_8.DS_CAN[i].mlstg1[3]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].kwh1[0]+dsatz_can[0].DS_CAN_8.DS_CAN[i].kwh1[1]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].mwh1[0]+dsatz_can[0].DS_CAN_8.DS_CAN[i].mwh1[1]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].mlstg2[0]+dsatz_can[0].DS_CAN_8.DS_CAN[i].mlstg2[1]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].mlstg2[2]+dsatz_can[0].DS_CAN_8.DS_CAN[i].mlstg2[3]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].kwh2[0]+dsatz_can[0].DS_CAN_8.DS_CAN[i].kwh2[1]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].mwh2[0]+dsatz_can[0].DS_CAN_8.DS_CAN[i].mwh2[1]+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].datum_zeit.sek+dsatz_can[0].DS_CAN_8.DS_CAN[i].datum_zeit.min+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].datum_zeit.std+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].datum_zeit.tag+dsatz_can[0].DS_CAN_8.DS_CAN[i].datum_zeit.monat+
             dsatz_can[0].DS_CAN_8.DS_CAN[i].datum_zeit.jahr;
		   }
		   retval += dsatz_can[0].DS_CAN_8.zeitstempel[0]+dsatz_can[0].DS_CAN_8.zeitstempel[1]+dsatz_can[0].DS_CAN_8.zeitstempel[2];
           return retval % modTeiler;
  }
  
  return retval;  
}

/* Berechnung der Pruefsumme fuer UVR61-3 */
int berechnepruefziffer_uvr61_3(u_DS_UVR1611_UVR61_3 ds_uvr61_3[])
{
  unsigned modTeiler;
  modTeiler = 0x100;    /* modTeiler = 256; */

  int retval=0;

  int k=0;
  for (k=0;k<6;k++)
    retval+= (ds_uvr61_3[0].DS_UVR61_3.sensT[k][0]+ds_uvr61_3[0].DS_UVR61_3.sensT[k][1]);

    retval+= ds_uvr61_3[0].DS_UVR61_3.ausgbyte1+
    ds_uvr61_3[0].DS_UVR61_3.dza+ds_uvr61_3[0].DS_UVR61_3.ausg_analog+
    ds_uvr61_3[0].DS_UVR61_3.wmzaehler_reg+
    ds_uvr61_3[0].DS_UVR61_3.volstrom[0]+ds_uvr61_3[0].DS_UVR61_3.volstrom[1]+
    ds_uvr61_3[0].DS_UVR61_3.mlstg1[0]+ds_uvr61_3[0].DS_UVR61_3.mlstg1[1]+
    ds_uvr61_3[0].DS_UVR61_3.kwh1[0]+ds_uvr61_3[0].DS_UVR61_3.kwh1[1]+
    ds_uvr61_3[0].DS_UVR61_3.mwh1[0]+ds_uvr61_3[0].DS_UVR61_3.mwh1[1]+
    ds_uvr61_3[0].DS_UVR61_3.mwh1[2]+ds_uvr61_3[0].DS_UVR61_3.mwh1[3]+
    ds_uvr61_3[0].DS_UVR61_3.datum_zeit.sek+ds_uvr61_3[0].DS_UVR61_3.datum_zeit.min+
    ds_uvr61_3[0].DS_UVR61_3.datum_zeit.std+
    ds_uvr61_3[0].DS_UVR61_3.datum_zeit.tag+ds_uvr61_3[0].DS_UVR61_3.datum_zeit.monat+
    ds_uvr61_3[0].DS_UVR61_3.datum_zeit.jahr+
    ds_uvr61_3[0].DS_UVR61_3.zeitstempel[0]+ds_uvr61_3[0].DS_UVR61_3.zeitstempel[1]+
    ds_uvr61_3[0].DS_UVR61_3.zeitstempel[2];

  return retval % modTeiler;
}

/* Berechnung der Pruefsumme fuer Modus D1 (2DL) */
int berechnepruefziffer_modus_D1(u_modus_D1 *ds_modus_D1, int anzahl)
{
  unsigned modTeiler;
  modTeiler = 0x100;    /* modTeiler = 256; */

  int retval=0;

  int k=0;
  for (k=0;k<anzahl-1;k++)
    retval+= ds_modus_D1[0].DS_alles.all_bytes[k];

  return retval % modTeiler;
}

/* Ermittlung Anzahl der Datensaetze Modus 0xD1 */
int anzahldatensaetze_D1(KopfsatzD1 kopf[])
{
  /* UCHAR byte1, byte2, byte3; */
  int byte1, byte2, byte3;

  /* Byte 1 - lowest */
  switch (kopf[0].endadresse[0])
    {
    case 0x0  : byte1 = 1; break;
    case 0x80 : byte1 = 2; break;
    default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
      return -1;
    }

  /* Byte 2 - mitte */
  byte2 = ((kopf[0].endadresse[1] / 0x02) * 0x02);

  /* Byte 3 - highest */
  byte3 = (kopf[0].endadresse[2] * 0x100)*0x02;

  if ( *end_adresse < *start_adresse || *(end_adresse+1) < *(start_adresse+1) || *(end_adresse+2) < *(start_adresse+2) ) 
	return 4096; // max. Anzahl Datenrahmen bei UVR1611 bzw. UVR61-3, Speicherueberlauf
  else
	return byte1 + byte2 + byte3;
}

/* Ermittlung Anzahl der Datensaetze Modus 0xDC (CAN-Logging) */
int anzahldatensaetze_DC(KOPFSATZ_DC kopf[])
{
  /* UCHAR byte1, byte2, byte3; */
  int byte1, byte2, byte3, return_byte, return_byte_max;
  
  switch(kopf[0].all_bytes[5])
  {
    case 1:
	  if (kopf[0].DC_Rahmen1.endadresse[0] == kopf[0].DC_Rahmen1.startadresse[0] &&
	  kopf[0].DC_Rahmen1.endadresse[1] == kopf[0].DC_Rahmen1.startadresse[1] &&
	  kopf[0].DC_Rahmen1.endadresse[2] == kopf[0].DC_Rahmen1.startadresse[2] && 
	  kopf[0].DC_Rahmen1.endadresse[0] == 0xFF && kopf[0].DC_Rahmen1.endadresse[1] == 0xFF && kopf[0].DC_Rahmen1.endadresse[2] == 0xFF )
	  {
	     printf("Keine geloggten Daten verfuegbar!\n");
		 return -2;
	  }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen1.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
      /* Byte 2 - mitte */
      byte2 = ((kopf[0].DC_Rahmen1.endadresse[1] / 0x02) * 0x04);
      /* Byte 3 - highest */
      byte3 = (kopf[0].DC_Rahmen1.endadresse[2] * 0x200);
	  return_byte = byte1 + byte2 + byte3;
	  return_byte_max = 8192;
      break;
    case 2:
	  if (kopf[0].DC_Rahmen2.endadresse[0] == kopf[0].DC_Rahmen2.startadresse[0] &&
	  kopf[0].DC_Rahmen2.endadresse[1] == kopf[0].DC_Rahmen2.startadresse[1] &&
	  kopf[0].DC_Rahmen2.endadresse[2] == kopf[0].DC_Rahmen2.startadresse[2]  && 
	  kopf[0].DC_Rahmen2.endadresse[0] == 0xFF && kopf[0].DC_Rahmen2.endadresse[1] == 0xFF && kopf[0].DC_Rahmen2.endadresse[2] == 0xFF )
	  {
	     printf("Keine geloggten Daten verfuegbar!\n");
		 return -2;
	  }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen2.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
	  return_byte = ((kopf[0].DC_Rahmen2.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen2.endadresse[1] /2 )* 4 + (byte1 -1)) / 2 ; // + 1;
	  return_byte_max = 4096;
      break;
    case 3:
	  if (kopf[0].DC_Rahmen3.endadresse[0] == kopf[0].DC_Rahmen3.startadresse[0] &&
	  kopf[0].DC_Rahmen3.endadresse[1] == kopf[0].DC_Rahmen3.startadresse[1] &&
	  kopf[0].DC_Rahmen3.endadresse[2] == kopf[0].DC_Rahmen3.startadresse[2]  && 
	  kopf[0].DC_Rahmen3.endadresse[0] == 0xFF && kopf[0].DC_Rahmen3.endadresse[1] == 0xFF && kopf[0].DC_Rahmen3.endadresse[2] == 0xFF )
	  {
	     printf("Keine geloggten Daten verfuegbar!\n");
		 return -2;
	  }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen3.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
	  return_byte = ((kopf[0].DC_Rahmen3.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen3.endadresse[1] /2 )* 4 + (byte1 -1)) / 3 ; // + 1;
	  return_byte_max = 2731;
      break;
    case 4:
	  if (kopf[0].DC_Rahmen4.endadresse[0] == kopf[0].DC_Rahmen4.startadresse[0] &&
	  kopf[0].DC_Rahmen4.endadresse[1] == kopf[0].DC_Rahmen4.startadresse[1] &&
	  kopf[0].DC_Rahmen4.endadresse[2] == kopf[0].DC_Rahmen4.startadresse[2]  && 
	  kopf[0].DC_Rahmen4.endadresse[0] == 0xFF && kopf[0].DC_Rahmen4.endadresse[1] == 0xFF && kopf[0].DC_Rahmen4.endadresse[2] == 0xFF )
	  {
	     printf("Keine geloggten Daten verfuegbar!\n");
		 return -2;
	  }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen4.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
	  return_byte = ((kopf[0].DC_Rahmen4.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen4.endadresse[1] /2 )* 4 + (byte1 -1)) / 4 ; // + 1;
	  return_byte_max = 2048;
      break;
    case 5:
	  if (kopf[0].DC_Rahmen5.endadresse[0] == kopf[0].DC_Rahmen5.startadresse[0] &&
	  kopf[0].DC_Rahmen5.endadresse[1] == kopf[0].DC_Rahmen5.startadresse[1] &&
	  kopf[0].DC_Rahmen5.endadresse[2] == kopf[0].DC_Rahmen5.startadresse[2]  && 
	  kopf[0].DC_Rahmen5.endadresse[0] == 0xFF && kopf[0].DC_Rahmen5.endadresse[1] == 0xFF && kopf[0].DC_Rahmen5.endadresse[2] == 0xFF )
	  {
	     printf("Keine geloggten Daten verfuegbar!\n");
		 return -2;
	  }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen5.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
	  return_byte = ((kopf[0].DC_Rahmen5.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen5.endadresse[1] /2 )* 4 + (byte1 -1)) / 5 ; // + 1;
	  return_byte_max = 1643;
      break;
    case 6:
	  if (kopf[0].DC_Rahmen6.endadresse[0] == kopf[0].DC_Rahmen6.startadresse[0] &&
	  kopf[0].DC_Rahmen6.endadresse[1] == kopf[0].DC_Rahmen6.startadresse[1] &&
	  kopf[0].DC_Rahmen6.endadresse[2] == kopf[0].DC_Rahmen6.startadresse[2]  && 
	  kopf[0].DC_Rahmen6.endadresse[0] == 0xFF && kopf[0].DC_Rahmen6.endadresse[1] == 0xFF && kopf[0].DC_Rahmen6.endadresse[2] == 0xFF )
	  {
	     printf("Keine geloggten Daten verfuegbar!\n");
		 return -2;
	  }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen6.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
	  return_byte = ((kopf[0].DC_Rahmen6.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen6.endadresse[1] /2 )* 4 + (byte1 -1)) / 6 ; // + 1;
	  return_byte_max = 1376;
      break;
    case 7:
	  if (kopf[0].DC_Rahmen7.endadresse[0] == kopf[0].DC_Rahmen7.startadresse[0] &&
	  kopf[0].DC_Rahmen7.endadresse[1] == kopf[0].DC_Rahmen7.startadresse[1] &&
	  kopf[0].DC_Rahmen7.endadresse[2] == kopf[0].DC_Rahmen7.startadresse[2]  && 
	  kopf[0].DC_Rahmen7.endadresse[0] == 0xFF && kopf[0].DC_Rahmen7.endadresse[1] == 0xFF && kopf[0].DC_Rahmen7.endadresse[2] == 0xFF )
	  {
	     printf("Keine geloggten Daten verfuegbar!\n");
		 return -2;
	  }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen7.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
	  return_byte = ((kopf[0].DC_Rahmen7.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen7.endadresse[1] /2 )* 4 + (byte1 -1)) / 7 ; //+ 1;
	  return_byte_max = 1174;
      break;
    case 8:
	  if (kopf[0].DC_Rahmen8.endadresse[0] == kopf[0].DC_Rahmen8.startadresse[0] &&
	  kopf[0].DC_Rahmen8.endadresse[1] == kopf[0].DC_Rahmen8.startadresse[1] &&
	  kopf[0].DC_Rahmen8.endadresse[2] == kopf[0].DC_Rahmen8.startadresse[2]  && 
	  kopf[0].DC_Rahmen8.endadresse[0] == 0xFF && kopf[0].DC_Rahmen8.endadresse[1] == 0xFF && kopf[0].DC_Rahmen8.endadresse[2] == 0xFF )
	  {
	     printf("Keine geloggten Daten verfuegbar!\n");
		 return -2;
	  }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen8.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
	  return_byte = ((kopf[0].DC_Rahmen8.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen8.endadresse[1] /2 )* 4 + (byte1 -1)) / 8 ; // + 1;
	  return_byte_max = 1024;
	  break;
  }

  if ( *end_adresse < *start_adresse || *(end_adresse+1) < *(start_adresse+1) || *(end_adresse+2) < *(start_adresse+2) ) 
	return return_byte_max; // max. Anzahl Datenrahmen bei UVR1611 bzw. UVR61-3, Speicherueberlauf
  else
    return return_byte;
}

/* Ermittlung Anzahl der Datensaetze Modus 0xA8 */
int anzahldatensaetze_A8(KopfsatzA8 kopf[])
{
  /* UCHAR byte1, byte2, byte3; */
  int byte1, byte2, byte3;

  /* Byte 1 - lowest */
  switch (kopf[0].endadresse[0])
    {
    case 0x0  : byte1 = 1; break;
    case 0x40 : byte1 = 2; break;
    case 0x80 : byte1 = 3; break;
    case 0xc0 : byte1 = 4; break;
    default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
      return -1;
    }

  /* Byte 2 - mitte */
  byte2 = ((kopf[0].endadresse[1] / 0x02) * 0x04);

  /* Byte 3 - highest */
  byte3 = (kopf[0].endadresse[2] * 0x100)*0x02;

  if ( *end_adresse < *start_adresse || *(end_adresse+1) < *(start_adresse+1) || *(end_adresse+2) < *(start_adresse+2) ) 
	return 8192; // max. Anzahl Datenrahmen bei UVR1611 bzw. UVR61-3, Speicherueberlauf
  else
	return byte1 + byte2 + byte3;
}

/* Datenpuffer im D-LOGG zuruecksetzen -USB */
int reset_datenpuffer_usb(int do_reset )
{
  int result = 0, sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[256];

  sendbuf[0]=ENDELESEN;   /* Senden "Ende lesen" */
   write_erg=write(fd_serialport,sendbuf,1);
   if (write_erg == 1)    /* Lesen der Antwort*/
     result=read(fd_serialport,empfbuf,1);
   /* Hier fertig mit "Ende lesen" */

  zeitstempel();

  if ( (empfbuf[0] == ENDELESEN) && (do_reset == 1) )
  {
    sendbuf[0]=RESETDATAFLASH;   /* Senden Buffer zuruecksetzen */
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      result=read(fd_serialport,empfbuf,1);
      /* printf("Vom DL erhaltene Reset-Bestaetigung: %x\n",empfbuf[0]); */
      fprintf(fp_varlogfile,"%s - %s -- Vom DL erhaltene Reset-Bestaetigung: %x.\n",sDatum, sZeit,empfbuf[0]);
    }
    else
      printf("Reset konnte nicht gesendet werden. Ergebnis = %d\n",result);
  }
  else
  {
     /* printf("Kein Data-Reset! \n"); */ /* reset-variable=%d \n",reset); */
    fprintf(fp_varlogfile,"%s - %s -- Kein Data-Reset! \n",sDatum, sZeit);
  }
  return 1;
}

/* Datenpuffer im D-LOGG zuruecksetzen -IP */
int reset_datenpuffer_ip(int do_reset )
{
  int result = 0, sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[256];

  sendbuf[0]=ENDELESEN;   /* Senden "Ende lesen" */
  if (!ip_first)
  {
    sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
     }  /* if (!ip_first) */
     write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
   /* Hier fertig mit "Ende lesen" */

  zeitstempel();

  if ( (empfbuf[0] == ENDELESEN) && (do_reset == 1) )
  {
    sendbuf[0]=RESETDATAFLASH;   /* Senden Buffer zuruecksetzen */
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      result  = recv(sock,empfbuf,1,0);
      /* printf("Vom DL erhaltene Reset-Bestaetigung: %x\n",empfbuf[0]); */
      fprintf(fp_varlogfile,"%s - %s -- Vom DL erhaltene Reset-Bestaetigung: %x.\n",sDatum, sZeit,empfbuf[0]);
    }
    else
      printf("Reset konnte nicht gesendet werden. Ergebnis = %d\n",result);
  }
  else
  {
     /* printf("Kein Data-Reset! \n"); */ /* reset-variable=%d \n",reset); */
    fprintf(fp_varlogfile,"%s - %s -- Kein Data-Reset! \n",sDatum, sZeit);
  }
  return 1;
}

/* aktuelles Datum und aktuelle Zeit als String fuer Log erzeugen*/
void zeitstempel(void)
{
  struct tm *zeit;
  time_t sekunde;

  time(&sekunde);
  zeit = localtime(&sekunde);

  strftime(sDatum, 10, "%x", zeit);
  strftime(sZeit, 10, "%X", zeit);
}

/* Berechne die Temperatur / Volumenstrom / Strahlung */
float berechnetemp(UCHAR lowbyte, UCHAR highbyte)
{
  UCHAR temp_highbyte;
  int z;
  short wert;
int sensor=0; //!!!!!!!!!!!!!
  temp_highbyte = highbyte;
  wert = lowbyte | ((highbyte & 0x0f)<<8);

  if( (highbyte & UVR1611) != 0 )
    {
      wert = wert ^ 0xfff;
      wert = -wert -1 ;
      return ((float) wert / 10);
    }
  else
    {
      if( sensor == 7)  // Raumtemperatur
      {
        if( (highbyte & 0x01) != 0 )
          return((256 + (float)lowbyte) / 10); /* Temperatur in C */
        else
          return(((float)lowbyte) / 10); /* Temperatur in C */
      }
      else
      {
        for(z=5;z<9;z++)
          temp_highbyte = temp_highbyte & ~(1 << z); /* die oberen 4 Bit auf 0 setzen */
        if (sensor == 6)
          return(((float)temp_highbyte*256) + (float)lowbyte); /* Strahlung in W/m2 */
        else
          return((((float)temp_highbyte*256) + (float)lowbyte) / 10); /* Temperatur in C */
      }
    }
}

/* Berechne Volumenstrom */
float berechnevol(UCHAR lowbyte, UCHAR highbyte)
{
  UCHAR temp_highbyte;
  int z;
  short wert;

  temp_highbyte = highbyte;
  wert = lowbyte | ((highbyte & 0x0f)<<8);

  //  if( (highbyte & UVR1611) != 0 )
  if( (highbyte & UVR1611) == 0 ) /* Volumenstrom kann nicht negativ sein*/
    {
      for(z=5;z<9;z++)
  temp_highbyte = temp_highbyte & ~(1 << z); /* die oberen 4 Bit auf 0 setzen */

      wert = (lowbyte | ((temp_highbyte & 0x0f)<<8)) * 4;
      return ((float) wert );
    }
  else
    {
      return( 0 ); /* Volumenstrom in l/h */
    }
}

/* Loesche ein bestimmtes Bit */
int clrbit( int word, int bit )
{
  return (word & ~(1 << bit));
}

/* Teste, ob ein bestimmtes Bit 0 oder 1 ist */
int tstbit( int word, int bit )
{
  return ((word & (1 << bit)) != 0);
}

/* Setze ein bestimmtes Bit */
int setbit( int word, int bit )
{
  return (word | (1 << bit));
}

/* Invertiere ein bestimmtes Bit */
int xorbit( int word, int bit )
{
  return (word ^ (1 << bit));
}

/* Abfrage per IP */
int ip_handling(int sock)
{
  unsigned char empfbuf[256];
  int send_bytes, recv_bytes;
  int sendbuf[1];

  sendbuf[0] = 0x81; /* Modusabfrage */

  send_bytes = send(sock, sendbuf,1,0);
  if (send_bytes == -1)
    return -1;

  recv_bytes = recv(sock, empfbuf,1,0);
  if (recv_bytes == -1)
    return -1;

  uvr_modus = empfbuf[0];
  //  printf("\nModus gelesen: %.2hX \n", uvr_modus);

  return 0;
}
