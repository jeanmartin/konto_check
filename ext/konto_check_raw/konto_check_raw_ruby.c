/*==================================================================
 *
 * KontoCheck Module, C Ruby Extension
 *
 * Copyright (c) 2010 Provideal Systems GmbH
 *
 * Peter Horn, peter.horn@provideal.net
 *
 * some extensions by Michael Plugge, m.plugge@hs-mannheim.de
 *
 * ------------------------------------------------------------------
 *
 * ACKNOWLEDGEMENT
 *
 * This module is entirely based on the C library konto_check
 * http://www.informatik.hs-mannheim.de/konto_check/
 * http://sourceforge.net/projects/kontocheck/
 * by Michael Plugge.
 *
 * ------------------------------------------------------------------
 *
 * LICENCE
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Include the Ruby headers and goodies
#include "ruby.h"
#include <stdio.h>
#include "konto_check.h"

   /* Ruby 1.8/1.9 compatibility definitions */
#ifndef RSTRING_PTR
#define RSTRING_LEN(x) (RSTRING(x)->len)
#define RSTRING_PTR(x) (RSTRING(x)->ptr)
#endif

#ifndef RUBY_T_STRING
#define RUBY_T_STRING T_STRING
#define RUBY_T_FLOAT  T_FLOAT
#define RUBY_T_FIXNUM T_FIXNUM
#define RUBY_T_BIGNUM T_BIGNUM
#define RUBY_T_ARRAY  T_ARRAY
#endif

#define RUNTIME_ERROR(error) do{ \
   snprintf(error_msg,511,"KontoCheck::%s, %s",kto_check_retval2txt_short(error),kto_check_retval2txt(error)); \
   rb_raise(rb_eRuntimeError, "%s", error_msg); \
}while(0)

// Defining a space for information and references about the module to be stored internally
VALUE KontoCheck = Qnil;

/**
 * get_params_file()
 *
 * extract params from argc/argv (filename and sometimes optional parameters)
 */
static void get_params_file(int argc,VALUE* argv,char *arg1s,int *arg1i,int *arg2i,int opts)
{
   int len;
   VALUE v1_rb,v2_rb,v3_rb;

   switch(opts){
      case 1:  /* ein Dateiname, zwei Integer; alle optional (f�r lut_init) */
         rb_scan_args(argc,argv,"03",&v1_rb,&v2_rb,&v3_rb);
         if(NIL_P(v2_rb))  /* Level */
            *arg1i=DEFAULT_INIT_LEVEL;
         else
            *arg1i=NUM2INT(v2_rb);
         if(NIL_P(v3_rb))  /* Set */
            *arg2i=0;
         else
            *arg2i=NUM2INT(v3_rb);
         break;

      case 2:  /* ein Dateiname (f�r dump_lutfile) */
         rb_scan_args(argc,argv,"10",&v1_rb);
         break;

      case 3:  /* ein optionaler Dateiname (f�r lut_info) */
         rb_scan_args(argc,argv,"01",&v1_rb);
         break;

      default:
         break;
   }
   if(NIL_P(v1_rb)){    /* Leerstring zur�ckgeben => Defaultwerte probieren */
      *arg1s=0;
   }
   else if(TYPE(v1_rb)==RUBY_T_STRING){
      strncpy(arg1s,RSTRING_PTR(v1_rb),FILENAME_MAX);
         /* der Ruby-String ist nicht notwendig null-terminiert; manuell erledigen */
      if((len=RSTRING_LEN(v1_rb))>FILENAME_MAX)len=FILENAME_MAX;
      *(arg1s+len)=0;
   }
   else
      rb_raise(rb_eRuntimeError, "%s", "Unable to convert given filename.");
}

/**
 * get_params_int()
 *
 * extract two numeric params from argc/argv (for integer search functions)
 */
static void get_params_int(int argc,VALUE* argv,int *arg1,int *arg2)
{
   char buffer[16];
   int len,cnt;
   VALUE arg1_rb,arg2_rb,*arr_ptr;

   rb_scan_args(argc,argv,"11",&arg1_rb,&arg2_rb);

         /* Falls als erster Parameter ein Array �bergeben wird, wird der erste
          * Wert werden als Minimal- und der zweite als Maximalwert genommen.
          */
   if(TYPE(arg1_rb)==RUBY_T_ARRAY){
      cnt=RARRAY_LEN(arg1_rb);      /* Anzahl Werte */
      arr_ptr=RARRAY_PTR(arg1_rb);  /* Pointer auf die Array-Daten */
      switch(cnt){
         case 0:
            arg1_rb=arg2_rb=Qnil;
            break;
         case 1:
            arg1_rb=*arr_ptr;
            arg2_rb=Qnil;
            break;
         default:
            arg1_rb=arr_ptr[0];
            arg2_rb=arr_ptr[1];
            break;
      }
   }

   if(NIL_P(arg1_rb))
      *arg1=0;
   else if(TYPE(arg1_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(arg1_rb),15);
         /* der Ruby-String ist nicht notwendig null-terminiert; manuell erledigen */
      if((len=RSTRING_LEN(arg1_rb))>15)len=15;
      *(buffer+len)=0;
      *arg1=atoi(buffer);
   }
   else
      *arg1=NUM2INT(arg1_rb);

   if(NIL_P(arg2_rb))
      *arg2=*arg1;
   else if(TYPE(arg2_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(arg2_rb),15);
      if((len=RSTRING_LEN(arg2_rb))>15)len=15;
      *(buffer+len)=0;
      *arg2=atoi(buffer);
   }
   else
      *arg2=NUM2INT(arg2_rb);
}

/**
 * get_params()
 *
 * extract params from argc/argv, convert&check results
 */
static void get_params(int argc,VALUE* argv,char *arg1s,char *arg2s,int *argi,int optargs)
{
   int len,maxlen=9;
   VALUE arg1_rb,arg2_rb;

   switch(optargs){
      case 0:  /* ein notwendiger Parameter (f�r lut_filialen) */
         rb_scan_args(argc,argv,"10",&arg1_rb);
         break;

      case 1:  /* ein notwendiger, ein optionaler Parameter (f�r viele lut-Funktionen) */
         rb_scan_args(argc,argv,"11",&arg1_rb,&arg2_rb);
         if(NIL_P(arg2_rb))   /* Filiale; Hauptstelle ist 0 */
            *argi=0;
         else
            *argi=NUM2INT(arg2_rb);
         break;

      case 2:  /* zwei notwendige Parameter (f�r konto_check) */
         rb_scan_args(argc,argv,"20",&arg1_rb,&arg2_rb);
         break;

      case 3:  /* ein notwendiger Parameter (f�r iban_check) */
         rb_scan_args(argc,argv,"10",&arg1_rb);
         maxlen=128;
         break;

      case 4:  /* ein notwendiger Parameter (f�r ipi_gen) */
         rb_scan_args(argc,argv,"10",&arg1_rb);
         maxlen=24;
         break;

      default:
         break;
   }

   switch(TYPE(arg1_rb)){
      case RUBY_T_STRING:
         strncpy(arg1s,RSTRING_PTR(arg1_rb),maxlen);
         if((len=RSTRING_LEN(arg1_rb))>maxlen)len=maxlen;
         *(arg1s+len)=0;
         break;
      case RUBY_T_FLOAT:
      case RUBY_T_FIXNUM:
      case RUBY_T_BIGNUM:
            /* Zahlwerte werden zun�chst nach double umgewandelt, da der
             * Zahlenbereich von 32 Bit Integers nicht gro� genug ist f�r
             * z.B. Kontonummern (10 Stellen). Mit snprintf wird dann eine
             * Stringversion erzeugt - nicht schnell aber einfach :-).
             */
         snprintf(arg1s,maxlen,"%5.0f",NUM2DBL(arg1_rb));
         break;
      default:
         if(!optargs)
            rb_raise(rb_eRuntimeError, "%s", "Unable to convert given blz.");
         else
            rb_raise(rb_eRuntimeError, "%s", "Unable to convert given value.");
         break;
   }
   if(optargs==2)switch(TYPE(arg2_rb)){  /* f�r konto_check(): kto holen */
      case RUBY_T_STRING:
         strncpy(arg2s,RSTRING_PTR(arg2_rb),15);
         if((len=RSTRING_LEN(arg2_rb))>15)len=15;
         *(arg2s+len)=0;
         break;
      case RUBY_T_FLOAT:
      case RUBY_T_FIXNUM:
      case RUBY_T_BIGNUM:
         snprintf(arg2s,16,"%5.0f",NUM2DBL(arg2_rb));
         break;
      default:
         rb_raise(rb_eRuntimeError, "%s", "Unable to convert given kto.");
         break;
   }
}

/**
 * KontoCheck::konto_check(<blz>, <kto>)
 *
 * check whether the given account number kto kann possibly be
 * a valid number of the bank with the bic blz.
 *
 * possible return values (and short description):
 *
 *    LUT2_NOT_INITIALIZED    "die Programmbibliothek wurde noch nicht initialisiert"
 *    MISSING_PARAMETER       "Bei der Kontopr�fung fehlt ein notwendiger Parameter (BLZ oder Konto)"
 *    
 *    NOT_IMPLEMENTED         "die Methode wurde noch nicht implementiert"
 *    UNDEFINED_SUBMETHOD     "Die (Unter)Methode ist nicht definiert"
 *    INVALID_BLZ             "Die (Unter)Methode ist nicht definiert"
 *    INVALID_BLZ_LENGTH      "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_KTO             "das Konto ist ung�ltig"
 *    INVALID_KTO_LENGTH      "ein Konto mu� zwischen 1 und 10 Stellen haben"
 *    FALSE                   "falsch"
 *    NOT_DEFINED             "die Methode ist nicht definiert"
 *    BAV_FALSE               "BAV denkt, das Konto ist falsch (konto_check h�lt es f�r richtig)"
 *    OK                      "ok"
 *    OK_NO_CHK               "ok, ohne Pr�fung"
 */
static VALUE konto_check(int argc,VALUE* argv,VALUE self)
{
   char kto[16],blz[16],error_msg[512];
   int retval;

   get_params(argc,argv,blz,kto,NULL,2);
   if((retval=kto_check_blz(blz,kto))==LUT2_NOT_INITIALIZED || retval==MISSING_PARAMETER)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval>0?Qtrue:Qfalse,INT2FIX(retval));
}


/**
 * KontoCheck::init([lutfile[,level[,set]]])
 *
 * initialize the underlying C library konto_check with the bank
 * information from lutfile.
 *
 * possible return values (and short description):
 *
 *    FILE_READ_ERROR         "kann Datei nicht lesen"
 *    NO_LUT_FILE             "die blz.lut Datei wurde nicht gefunden"
 *    INCREMENTAL_INIT_NEEDS_INFO            "Ein inkrementelles Initialisieren ben�tigt einen Info-Block in der LUT-Datei"
 *    INCREMENTAL_INIT_FROM_DIFFERENT_FILE   "Ein inkrementelles Initialisieren mit einer anderen LUT-Datei ist nicht m�glich"
 *    LUT2_BLOCK_NOT_IN_FILE  "Der Block ist nicht in der LUT-Datei enthalten"
 *    INIT_FATAL_ERROR        "Initialisierung fehlgeschlagen (init_wait geblockt)"
 *    ERROR_MALLOC            "kann keinen Speicher allokieren"
 *    INVALID_LUT_FILE        "die blz.lut Datei ist inkosistent/ung�ltig"
 *    LUT_CRC_ERROR           "Pr�fsummenfehler in der blz.lut Datei"
 *    LUT2_DECOMPRESS_ERROR   "Fehler beim Dekomprimieren eines LUT-Blocks"
 *    LUT2_FILE_CORRUPTED     "Die LUT-Datei ist korrumpiert"
 *    LUT2_Z_DATA_ERROR       "Datenfehler im komprimierten LUT-Block"
 *    LUT2_Z_MEM_ERROR        "Memory error in den ZLIB-Routinen"
 *    KTO_CHECK_UNSUPPORTED_COMPRESSION "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
 *
 *    LUT1_SET_LOADED         "Die Datei ist im alten LUT-Format (1.0/1.1)"
 *    OK                      "ok"
 */
static VALUE init(int argc,VALUE* argv,VALUE self)
{
   char lut_name[FILENAME_MAX+1],error_msg[512];
   int retval,level,set;

   get_params_file(argc,argv,lut_name,&level,&set,1);
   retval=lut_init(lut_name,level,set);
   switch(retval){
      case OK:
      case LUT1_SET_LOADED:
      case LUT2_PARTIAL_OK:
         break;
      default:
         RUNTIME_ERROR(retval);
   }
   return INT2FIX(retval);
}

/**
 * KontoCheck::free()
 *
 * release allocated memory of the underlying C library konto_check
 * return value is always true
 */
static VALUE free_rb(VALUE self)
{
   lut_cleanup();
   return Qtrue;
}

/**
 * KontoCheck::generate_lutfile()
 *
 * generate a new lutfile
 *
 * possible return values (and short description):
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    FILE_READ_ERROR            "kann Datei nicht lesen"
 *    FILE_WRITE_ERROR           "kann Datei nicht schreiben"
 *    INVALID_BLZ_FILE           "Fehler in der blz.txt Datei (falsche Zeilenl�nge)"
 *    INVALID_LUT_FILE           "die blz.lut Datei ist inkosistent/ung�ltig"
 *    KTO_CHECK_UNSUPPORTED_COMPRESSION "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
 *    LUT2_COMPRESS_ERROR        "Fehler beim Komprimieren eines LUT-Blocks"
 *    LUT2_FILE_CORRUPTED        "Die LUT-Datei ist korrumpiert"
 *    LUT2_GUELTIGKEIT_SWAPPED   "Im G�ltigkeitsdatum sind Anfangs- und Enddatum vertauscht"
 *    LUT2_INVALID_GUELTIGKEIT   "Das angegebene G�ltigkeitsdatum ist ung�ltig (Soll: JJJJMMTT-JJJJMMTT)"
 *    LUT2_NO_SLOT_FREE          "Im Inhaltsverzeichnis der LUT-Datei ist kein Slot mehr frei"
 *
 *    LUT1_FILE_GENERATED        "ok; es wurde allerdings eine LUT-Datei im alten Format (1.0/1.1) generiert"
 *    OK                         "ok"
 */
static VALUE generate_lutfile_rb(int argc,VALUE* argv,VALUE self)
{
   char input_name[FILENAME_MAX+1],output_name[FILENAME_MAX+1];
   char user_info[256],gueltigkeit[20],buffer[16],error_msg[512];
   int retval,felder,filialen,set,len;
   VALUE input_name_rb,output_name_rb,user_info_rb,
         gueltigkeit_rb,felder_rb,filialen_rb,set_rb;

   rb_scan_args(argc,argv,"25",&input_name_rb,&output_name_rb,
         &user_info_rb,&gueltigkeit_rb,&felder_rb,&filialen_rb,&set_rb);

   if(TYPE(input_name_rb)==RUBY_T_STRING){
      strncpy(input_name,RSTRING_PTR(input_name_rb),FILENAME_MAX);
      if((len=RSTRING_LEN(input_name_rb))>FILENAME_MAX)len=FILENAME_MAX;
      *(input_name+len)=0;
   }
   else
      rb_raise(rb_eRuntimeError, "%s", "Unable to convert given input filename.");

   if(TYPE(output_name_rb)==RUBY_T_STRING){
      strncpy(output_name,RSTRING_PTR(output_name_rb),FILENAME_MAX);
      if((len=RSTRING_LEN(output_name_rb))>FILENAME_MAX)len=FILENAME_MAX;
      *(output_name+len)=0;
   }
   else
      rb_raise(rb_eRuntimeError, "%s", "Unable to convert given output filename.");

   if(NIL_P(user_info_rb)){
      *user_info=0;
   }
   else if(TYPE(user_info_rb)==RUBY_T_STRING){
      strncpy(user_info,RSTRING_PTR(user_info_rb),255);
      if((len=RSTRING_LEN(user_info_rb))>255)len=255;
      *(user_info+len)=0;
   }
   else
      rb_raise(rb_eRuntimeError, "%s", "Unable to convert given user_info string.");

   if(NIL_P(gueltigkeit_rb)){
      *gueltigkeit=0;
   }
   else if(TYPE(gueltigkeit_rb)==RUBY_T_STRING){
      strncpy(gueltigkeit,RSTRING_PTR(gueltigkeit_rb),19);
      if((len=RSTRING_LEN(gueltigkeit_rb))>19)len=19;
      *(gueltigkeit+len)=0;
   }
   else
      rb_raise(rb_eRuntimeError, "%s", "Unable to convert given gueltigkeit string.");

   if(NIL_P(felder_rb))
      felder=DEFAULT_LUT_FIELDS_NUM;
   else if(TYPE(felder_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(felder_rb),15);
      if((len=RSTRING_LEN(felder_rb))>15)len=15;
      *(buffer+len)=0;
      felder=atoi(buffer);
   }
   else
      felder=NUM2INT(felder_rb);

   if(NIL_P(filialen_rb))
      filialen=0;
   else if(TYPE(filialen_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(filialen_rb),15);
      if((len=RSTRING_LEN(felder_rb))>15)len=15;
      *(buffer+len)=0;
      filialen=atoi(buffer);
   }
   else
      filialen=NUM2INT(filialen_rb);

   if(NIL_P(set_rb))
      set=0;
   else if(TYPE(set_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(set_rb),15);
      if((len=RSTRING_LEN(set_rb))>15)len=15;
      *(buffer+len)=0;
      set=atoi(buffer);
   }
   else
      set=NUM2INT(set_rb);

   retval=generate_lut2_p(input_name,output_name,user_info,gueltigkeit,felder,filialen,0,0,set);
   if(retval<0)RUNTIME_ERROR(retval);
   return INT2FIX(retval);
}

/**
 * KontoCheck::retval2txt(<retval>)
 *
 * convert a numeric retval to string in various encodings
 */
static VALUE retval2txt_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2txt(FIX2INT(retval)));
}

static VALUE retval2dos_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2dos(FIX2INT(retval)));
}

static VALUE retval2html_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2html(FIX2INT(retval)));
}

static VALUE retval2utf8_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2utf8(FIX2INT(retval)));
}

/**
 * KontoCheck::dump_lutfile(<lutfile>)
 *
 * get all infos about the contents of a lutfile
 */
static VALUE dump_lutfile_rb(int argc,VALUE* argv,VALUE self)
{
   char lut_name[FILENAME_MAX+1],*ptr;
   int retval;
   VALUE dump;

   get_params_file(argc,argv,lut_name,NULL,NULL,2);
   retval=lut_dir_dump_str(lut_name,&ptr);
   if(retval<=0)
      dump=Qnil;
   else
      dump=rb_str_new2(ptr);
   kc_free(ptr);  /* die C-Funktion allokiert Speicher, der mu� wieder freigegeben werden */
   return rb_ary_new3(2,dump,INT2FIX(retval));
}

/**
 * KontoCheck::lut_info([lutfile])
 *
 * get infos about the contents of a lutfile or the loaded blocks (call without filename)
 *
 * possible return values (and short description):
 *
 *    ERROR_MALLOC           "kann keinen Speicher allokieren"
 *    FILE_READ_ERROR        "kann Datei nicht lesen"
 *    INVALID_LUT_FILE       "die blz.lut Datei ist inkosistent/ung�ltig"
 *    KTO_CHECK_UNSUPPORTED_COMPRESSION "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
 *    LUT1_FILE_USED         "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_BLOCK_NOT_IN_FILE "Der Block ist nicht in der LUT-Datei enthalten"
 *    LUT2_FILE_CORRUPTED    "Die LUT-Datei ist korrumpiert"
 *    LUT2_NOT_INITIALIZED   "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_Z_BUF_ERROR       "Buffer error in den ZLIB Routinen"
 *    LUT2_Z_DATA_ERROR      "Datenfehler im komprimierten LUT-Block"
 *    LUT2_Z_MEM_ERROR       "Memory error in den ZLIB-Routinen"
 *    LUT_CRC_ERROR          "Pr�fsummenfehler in der blz.lut Datei"
 *    OK                     "ok"
 *
 */
static VALUE lut_info_rb(int argc,VALUE* argv,VALUE self)
{
   char lut_name[FILENAME_MAX+1],*info1,*info2,error_msg[512];
   int retval,valid1,valid2;
   VALUE info1_rb,info2_rb;

   get_params_file(argc,argv,lut_name,NULL,NULL,3);
   retval=lut_info(lut_name,&info1,&info2,&valid1,&valid2);
   if(!info1)
      info1_rb=Qnil;
   else
      info1_rb=rb_str_new2(info1);
   if(!info2)
      info2_rb=Qnil;
   else
      info2_rb=rb_str_new2(info2);
   kc_free(info1);  /* die C-Funktion allokiert Speicher, der mu� wieder freigegeben werden */
   kc_free(info2);
   if(retval<0)RUNTIME_ERROR(retval);
   return rb_ary_new3(5,INT2FIX(retval),INT2FIX(valid1),INT2FIX(valid2),info1_rb,info2_rb);
}

/**
 * KontoCheck::load_bank_data(<datafile>)
 *
 * This function was the old initialization function for konto_check; it is now
 * replaced by init() for initialization and generate_lutfile() for generating
 * a data file with all bank informations from blz_yyyymmdd.txt from
 * http://www.bundesbank.de/zahlungsverkehr/zahlungsverkehr_bankleitzahlen_download.php
 *
 * This function serves now only as a schibbolet to check if someone uses the
 * old interface :-).
 */

 /****************************************************************************
  ****************************************************************************
  ****                                                                    ****
  ****     WARNING - this function will be removed in near future.        ****
  ****     Use KontoCheck::init() for initialization from a lutfile,      ****
  ****     KontoCheck::generate_lutfile() to generate a new lutfile.      ****
  ****     The init() function is much faster (about 7..20 times)         ****
  ****     then this function and has some more advantages (two sets      ****
  ****     of data, flexible level of initialization, compact format).    ****
  ****                                                                    ****
  ****************************************************************************
  ****************************************************************************/

static VALUE load_bank_data(VALUE self, VALUE path_rb) {
   rb_raise(rb_eRuntimeError, "%s", "Perhaps you used the old interface of konto_check.\n"
         "Use KontoCheck::init() to initialize the library\n"
         "and check the order of function arguments for konto_test(blz,kto)");
}

/**
 * KontoCheck::iban_2bic(<blz>)
 *
 * determine the BIC for a given (german) IBAN
 *
 * possible return values for retval (and short description):
 *
 *    IBAN2BIC_ONLY_GERMAN       "Die Funktion iban2bic() arbeitet nur mit deutschen Bankleitzahlen"
 *    LUT2_BIC_NOT_INITIALIZED   "Das Feld BIC wurde nicht initialisiert"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    INVALID_BLZ_LENGTH         "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                "die Bankleitzahl ist ung�ltig"
 *    OK                         "ok"
 */
static VALUE iban2bic_rb(int argc,VALUE* argv,VALUE self)
{
   char iban[128],error_msg[512],blz[10],kto[16],*bic;
   int retval;

   get_params(argc,argv,iban,NULL,NULL,3);
   bic=iban2bic(iban,&retval,blz,kto);
   if(retval<0 && retval!=INVALID_BLZ)RUNTIME_ERROR(retval);
   return rb_ary_new3(4,rb_str_new2(bic),INT2FIX(retval),rb_str_new2(blz),rb_str_new2(kto));
}

/**
 * KontoCheck::iban_check(<blz>)
 *
 * test an IBAN
 *
 * possible return values for retval (and short description):
 *
 *    NO_GERMAN_BIC              "Ein Konto kann kann nur f�r deutsche Banken gepr�ft werden"
 *    IBAN_OK_KTO_NOT            "Die Pr�fziffer der IBAN stimmt, die der Kontonummer nicht"
 *    KTO_OK_IBAN_NOT            "Die Pr�fziffer der Kontonummer stimmt, die der IBAN nicht"
 *    FALSE                      "falsch"
 *    OK                         "ok"
 *
 * possible return values for retval_kc:
 *    LUT2_NOT_INITIALIZED    "die Programmbibliothek wurde noch nicht initialisiert"
 *    MISSING_PARAMETER       "Bei der Kontopr�fung fehlt ein notwendiger Parameter (BLZ oder Konto)"
 *    NOT_IMPLEMENTED         "die Methode wurde noch nicht implementiert"
 *    UNDEFINED_SUBMETHOD     "Die (Unter)Methode ist nicht definiert"
 *    INVALID_BLZ             "Die (Unter)Methode ist nicht definiert"
 *    INVALID_BLZ_LENGTH      "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_KTO             "das Konto ist ung�ltig"
 *    INVALID_KTO_LENGTH      "ein Konto mu� zwischen 1 und 10 Stellen haben"
 *    FALSE                   "falsch"
 *    NOT_DEFINED             "die Methode ist nicht definiert"
 *    BAV_FALSE               "BAV denkt, das Konto ist falsch (konto_check h�lt es f�r richtig)"
 *    OK                      "ok"
 *    OK_NO_CHK               "ok, ohne Pr�fung"
 */
static VALUE iban_check_rb(int argc,VALUE* argv,VALUE self)
{
   char iban[128];
   int retval,retval_kc;

   get_params(argc,argv,iban,NULL,NULL,3);
   retval=iban_check(iban,&retval_kc);
   return rb_ary_new3(2,INT2FIX(retval),INT2FIX(retval_kc));
}

/**
 * KontoCheck::ipi_gen(<zweck>)
 *
 * determine the BIC for a given (german) IBAN
 *
 * possible return values for retval (and short description):
 *
 *    IPI_INVALID_LENGTH         "Die L�nge des IPI-Verwendungszwecks darf maximal 18 Byte sein"
 *    IPI_INVALID_CHARACTER      "Im strukturierten Verwendungszweck d�rfen nur alphanumerische Zeichen vorkommen"
 *    OK                         "ok"
 */
static VALUE ipi_gen_rb(int argc,VALUE* argv,VALUE self)
{
   char zweck[24],dst[24],papier[30];
   int retval;

   get_params(argc,argv,zweck,NULL,NULL,4);
   retval=ipi_gen(zweck,dst,papier);
   return rb_ary_new3(3,rb_str_new2(dst),INT2FIX(retval),rb_str_new2(papier));
}

/**
 * KontoCheck::ipi_check(<zweck>)
 *
 * determine the BIC for a given (german) IBAN
 *
 * possible return values for retval (and short description):
 *
 *    IPI_CHECK_INVALID_LENGTH   "Der zu validierende strukturierete Verwendungszweck mu� genau 20 Zeichen enthalten"
 *    FALSE                      "falsch"
 *    OK                         "ok"
 */
static VALUE ipi_check_rb(int argc,VALUE* argv,VALUE self)
{
   char zweck[128];

   get_params(argc,argv,zweck,NULL,NULL,3);
   return INT2FIX(ipi_check(zweck));
}

/**
 * KontoCheck::bank_valid(<blz> [,filiale])
 *
 * check if a given blz is valid
 *
 * possible return values (and short description):
 *
 *    LUT2_INDEX_OUT_OF_RANGE    "Der Index f�r die Filiale ist ung�ltig"
 *    LUT2_BLZ_NOT_INITIALIZED   "Das Feld BLZ wurde nicht initialisiert"
 *    INVALID_BLZ_LENGTH         "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                "die Bankleitzahl ist ung�ltig"
 *    OK                         "ok"
 */
static VALUE bank_valid(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int filiale,retval;

   get_params(argc,argv,blz,NULL,&filiale,1);
   retval=lut_blz(blz,filiale);
   if(retval==LUT2_BLZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval>0?Qtrue:Qfalse,INT2FIX(retval));
}

/**
 * KontoCheck::bank_filialen(<blz>)
 *
 * return the number of branch offices
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_FILIALEN_NOT_INITIALIZED "Das Feld Filialen wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_filialen(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,cnt;

   get_params(argc,argv,blz,NULL,NULL,0);
   cnt=lut_filialen(blz,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_FILIALEN_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(cnt),INT2FIX(retval));
}

/**
 * KontoCheck::bank_alles(<blz> [,filiale])
 *
 * return all available information about a bank
 *
 * possible return values (and short description):
 *
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    INVALID_BLZ_LENGTH         "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                "die Bankleitzahl ist ung�ltig"
 *    LUT2_PARTIAL_OK            "es wurden nicht alle Blocks geladen"
 *    OK                         "ok"
 */

static VALUE bank_alles(int argc,VALUE* argv,VALUE self)
{
   char blz[16],**p_name,**p_name_kurz,**p_ort,**p_bic,*p_aenderung,*p_loeschung,aenderung[2],loeschung[2],error_msg[512];
   int retval,filiale,cnt,*p_blz,*p_plz,*p_pan,p_pz,*p_nr,*p_nachfolge_blz;

   get_params(argc,argv,blz,NULL,&filiale,1);
   retval=lut_multiple(blz,&cnt,&p_blz, &p_name,&p_name_kurz,&p_plz,&p_ort,&p_pan,&p_bic,&p_pz,&p_nr,
         &p_aenderung,&p_loeschung,&p_nachfolge_blz,NULL,NULL,NULL);
   if(retval==LUT2_BLZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   if(filiale<0 || (cnt && filiale>=cnt))return(INT2FIX(LUT2_INDEX_OUT_OF_RANGE));  /* ung�ltige Filiale */

      /* Fehler, die C-Arrays d�rfen nicht dereferenziert werden */
   if(retval<=0 && retval!=LUT2_PARTIAL_OK)return rb_ary_new3(2,INT2FIX(retval),Qnil);

   *aenderung=p_aenderung[filiale];
   *loeschung=p_loeschung[filiale];
   *(aenderung+1)=*(loeschung+1)=0;

#define SV(x) *x?rb_str_new2(x):Qnil
#define IV(x) x>=0?INT2FIX(x):Qnil
   return rb_ary_new3(13,INT2FIX(retval),IV(cnt),SV(p_name[filiale]),
         SV(p_name_kurz[filiale]),IV(p_plz[filiale]),SV(p_ort[filiale]),
         IV(p_pan[filiale]),SV(p_bic[filiale]),IV(p_pz),IV(p_nr[filiale]),
         SV(aenderung),SV(loeschung),IV(p_nachfolge_blz[filiale]));
}

/**
 * KontoCheck::bank_name(<blz> [,filiale])
 *
 * return the name of a bank
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_NAME_NOT_INITIALIZED     "Das Feld Bankname wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_name(int argc,VALUE* argv,VALUE self)
{
   char blz[16],*name,error_msg[512];
   int retval,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   name=lut_name(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_NAME_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(name),INT2FIX(retval));
}

/**
 * KontoCheck::bank_name_kurz(<blz> [,filiale])
 *
 * return the short name of a bank
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_NAME_KURZ_NOT_INITIALIZED "Das Feld Kurzname wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_name_kurz(int argc,VALUE* argv,VALUE self)
{
   char blz[16],*name,error_msg[512];
   int retval,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   name=lut_name_kurz(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_NAME_KURZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(name),INT2FIX(retval));
}

/**
 * KontoCheck::bank_name_ort(<blz> [,filiale])
 *
 * return the location of a bank
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_ORT_NOT_INITIALIZED      "Das Feld Ort wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_ort(int argc,VALUE* argv,VALUE self)
{
   char blz[16],*ort,error_msg[512];
   int retval,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   ort=lut_ort(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_ORT_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(ort),INT2FIX(retval));
}

/**
 * KontoCheck::bank_plz(<blz> [,filiale])
 *
 * return the plz of a bank
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_PLZ_NOT_INITIALIZED      "Das Feld PLZ wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_plz(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,plz,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   plz=lut_plz(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_PLZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(plz),INT2FIX(retval));
}

/**
 * KontoCheck::bank_pz(<blz> [,filiale])
 *
 * return the check method of a bank
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_PZ_NOT_INITIALIZED       "Das Feld Pr�fziffer wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_pz(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,pz,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   pz=lut_pz(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_PZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(pz),INT2FIX(retval));
}

/**
 * KontoCheck::bank_bic(<blz> [,filiale])
 *
 * return the BIC (Bank Identifier Code) of a bank
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_BIC_NOT_INITIALIZED      "Das Feld BIC wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_bic(int argc,VALUE* argv,VALUE self)
{
   char blz[16],*ptr,error_msg[512];
   int retval,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   ptr=lut_bic(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_BIC_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(ptr),INT2FIX(retval));
}

/**
 * KontoCheck::bank_aenderung(<blz> [,filiale])
 *
 * return the '�nderung' flag of a bank (as string):
 *    A Addition, M Modified, U Unchanged, D Deletion.
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_AENDERUNG_NOT_INITIALIZED "Das Feld �nderung wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_aenderung(int argc,VALUE* argv,VALUE self)
{
   char blz[16],aenderung[2],error_msg[512];
   int retval,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   *aenderung=lut_aenderung(blz,filiale,&retval);
   aenderung[1]=0;
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_AENDERUNG_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(aenderung),INT2FIX(retval));
}

/**
 * KontoCheck::bank_loeschung(<blz> [,filiale])
 *
 * return the 'L�schung' flag of a bank as int: 0 or 1
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_LOESCHUNG_NOT_INITIALIZED "Das Feld L�schung wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_loeschung(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,loeschung,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   loeschung=lut_loeschung(blz,filiale,&retval)-'0';
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_LOESCHUNG_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(loeschung),INT2FIX(retval));
}

/**
 * KontoCheck::bank_nachfolge_blz(<blz> [,filiale])
 *
 * return the successor of a bank which will be deleted ('L�schung' flag set)
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED "Das Feld Nachfolge-BLZ wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_nachfolge_blz(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,n_blz,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   n_blz=lut_nachfolge_blz(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(n_blz),INT2FIX(retval));
}

/**
 * KontoCheck::bank_pan(<blz> [,filiale])
 *
 * return the PAN (Primary Account Number) of a bank
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_PAN_NOT_INITIALIZED      "Das Feld PAN wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_pan(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,pan,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   pan=lut_pan(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_PAN_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(pan),INT2FIX(retval));
}

/**
 * KontoCheck::bank_nr(<blz> [,filiale])
 *
 * return the sequence number of a bank (internal field of BLZ file)
 *
 * possible return values (and short description):
 *
 *    LUT2_BLZ_NOT_INITIALIZED      "Das Feld BLZ wurde nicht initialisiert"
 *    LUT2_NR_NOT_INITIALIZED       "Das Feld NR wurde nicht initialisiert"
 *    LUT2_INDEX_OUT_OF_RANGE       "Der Index f�r die Filiale ist ung�ltig"
 *    INVALID_BLZ_LENGTH            "die Bankleitzahl ist nicht achtstellig"
 *    INVALID_BLZ                   "die Bankleitzahl ist ung�ltig"
 *    OK                            "ok"
 */
static VALUE bank_nr(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,nr,filiale;

   get_params(argc,argv,blz,NULL,&filiale,1);
   nr=lut_nr(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_NR_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(nr),INT2FIX(retval));
}

/**
 * Die Suchroutinen der C-Bibliothek sind eher Low-Level Routinen und ohne
 * einen Blick in die Sourcen nicht intuitiv nutzbar ;-), daher hier zun�chst
 * eine kurze Einf�hrung.
 *
 * Beim ersten Aufruf einer dieser Routinen wird zun�chst getestet, ob das
 * Array mit den Zweigstellen bereits erstellt wurde. Dieses Array
 * korrespondiert mit der Tabelle der Bankleitzahlen; es enth�lt zu jedem Index
 * die jeweilige Zweigstellennummer, wobei Hauptstellen den Wert 0 erhalten.
 * Das erste Element in diesem Array entspricht der Bundesbank.
 *
 * Als zweites wird getestet, ob das Array mit den sortierten Werten schon
 * vorhanden ist; falls nicht, wird Speicher allokiert, der entsprechende Block
 * sortiert und die Indizes in das Array eingetragen (es w�re zu testen, ob die
 * Initialisierung schneller ist, wenn man das Array in der LUT-Datei
 * speichert).
 *
 * Um einen bestimmten Wert zu finden, wird eine bin�re Suche durchgef�hrt.
 * Wird das gesuchte Element gefunden, werden die folgenden Werte zur�ckgegeben:
 *
 *    - anzahl:      Anzahl der gefundenen Werte, die den Suchkriterien entsprechen
 *
 *    - start_idx:   Pointer auf den ersten Wert im Sortierarray. Die n�chsten
 *                   <anzahl> Elemente ab diesem Element entsprechen den
 *                   Sortierkriterien. Die Werte geben dabei den Index innerhalb
 *                   der BLZ-Datei an, beginnend mit 0 f�r die Bundesbank.
 *
 *    - zweigstelle: Dieses Array enth�lt die Zweigstellennummern der
 *                   jeweiligen Banken. Es umfasst jedoch den Gesamtbestand der
 *                   BLZ-Datei, nicht den des Sortierarrays.
 *
 *    - base_name:   Dies ist ein Array das die Werte der Suchfunktion (in der
 *                   Reihenfolge der BLZ-Datei) enth�lt.
 *
 *    - blz_base:    Dies ist das Array mit den Bankleitzahlen.
 *
 * Beispiel: Der Aufruf
 *
 * retval=lut_suche_ort("aa",&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
 *
 * liefert das folgende Ergebnis (mit der LUT-Datei vom 7.3.2011; bei anderen LUT-Dateien
 * k�nnen sich die Indizes nat�rlich verschieben):
 *
 *    anzahl: 38
 *    i:  0, start_idx[ 0]:  9259, blz_base[ 9259]: 58550130, zweigstelle[ 9259]: 42, base_name[ 9259]: Aach b Trier
 *    i:  1, start_idx[ 1]: 12862, blz_base[12862]: 69251445, zweigstelle[12862]:  1, base_name[12862]: Aach, Hegau
 *    i:  2, start_idx[ 2]: 12892, blz_base[12892]: 69290000, zweigstelle[12892]:  5, base_name[12892]: Aach, Hegau
 *    i:  3, start_idx[ 3]:  3946, blz_base[ 3946]: 31010833, zweigstelle[ 3946]: 13, base_name[ 3946]: Aachen
 *    i:  4, start_idx[ 4]:  4497, blz_base[ 4497]: 37060590, zweigstelle[ 4497]:  3, base_name[ 4497]: Aachen
 *    i:  5, start_idx[ 5]:  4830, blz_base[ 4830]: 39000000, zweigstelle[ 4830]:  0, base_name[ 4830]: Aachen
 *    i:  6, start_idx[ 6]:  4831, blz_base[ 4831]: 39010111, zweigstelle[ 4831]:  0, base_name[ 4831]: Aachen
 *    i:  7, start_idx[ 7]:  4833, blz_base[ 4833]: 39020000, zweigstelle[ 4833]:  0, base_name[ 4833]: Aachen
 *    i:  8, start_idx[ 8]:  4834, blz_base[ 4834]: 39040013, zweigstelle[ 4834]:  0, base_name[ 4834]: Aachen
 *    i:  9, start_idx[ 9]:  4841, blz_base[ 4841]: 39050000, zweigstelle[ 4841]:  0, base_name[ 4841]: Aachen
 *    i: 10, start_idx[10]:  4851, blz_base[ 4851]: 39060180, zweigstelle[ 4851]:  0, base_name[ 4851]: Aachen
 *    i: 11, start_idx[11]:  4857, blz_base[ 4857]: 39060180, zweigstelle[ 4857]:  6, base_name[ 4857]: Aachen
 *    i: 12, start_idx[12]:  4858, blz_base[ 4858]: 39060630, zweigstelle[ 4858]:  0, base_name[ 4858]: Aachen
 *    i: 13, start_idx[13]:  4861, blz_base[ 4861]: 39070020, zweigstelle[ 4861]:  0, base_name[ 4861]: Aachen
 *    i: 14, start_idx[14]:  4872, blz_base[ 4872]: 39070024, zweigstelle[ 4872]:  0, base_name[ 4872]: Aachen
 *    i: 15, start_idx[15]:  4883, blz_base[ 4883]: 39080005, zweigstelle[ 4883]:  0, base_name[ 4883]: Aachen
 *    i: 16, start_idx[16]:  4889, blz_base[ 4889]: 39080098, zweigstelle[ 4889]:  0, base_name[ 4889]: Aachen
 *    i: 17, start_idx[17]:  4890, blz_base[ 4890]: 39080099, zweigstelle[ 4890]:  0, base_name[ 4890]: Aachen
 *    i: 18, start_idx[18]:  4891, blz_base[ 4891]: 39160191, zweigstelle[ 4891]:  0, base_name[ 4891]: Aachen
 *    i: 19, start_idx[19]:  4892, blz_base[ 4892]: 39161490, zweigstelle[ 4892]:  0, base_name[ 4892]: Aachen
 *    i: 20, start_idx[20]:  4896, blz_base[ 4896]: 39162980, zweigstelle[ 4896]:  3, base_name[ 4896]: Aachen
 *    i: 21, start_idx[21]:  4906, blz_base[ 4906]: 39500000, zweigstelle[ 4906]:  0, base_name[ 4906]: Aachen
 *    i: 22, start_idx[22]:  6333, blz_base[ 6333]: 50120383, zweigstelle[ 6333]:  1, base_name[ 6333]: Aachen
 *    i: 23, start_idx[23]: 10348, blz_base[10348]: 60420000, zweigstelle[10348]: 18, base_name[10348]: Aachen
 *    i: 24, start_idx[24]: 16183, blz_base[16183]: 76026000, zweigstelle[16183]:  1, base_name[16183]: Aachen
 *    i: 25, start_idx[25]: 10001, blz_base[10001]: 60069673, zweigstelle[10001]:  1, base_name[10001]: Aalen, W�rtt
 *    i: 26, start_idx[26]: 10685, blz_base[10685]: 61370024, zweigstelle[10685]:  1, base_name[10685]: Aalen, W�rtt
 *    i: 27, start_idx[27]: 10695, blz_base[10695]: 61370086, zweigstelle[10695]:  3, base_name[10695]: Aalen, W�rtt
 *    i: 28, start_idx[28]: 10714, blz_base[10714]: 61420086, zweigstelle[10714]:  0, base_name[10714]: Aalen, W�rtt
 *    i: 29, start_idx[29]: 10715, blz_base[10715]: 61430000, zweigstelle[10715]:  0, base_name[10715]: Aalen, W�rtt
 *    i: 30, start_idx[30]: 10716, blz_base[10716]: 61440086, zweigstelle[10716]:  0, base_name[10716]: Aalen, W�rtt
 *    i: 31, start_idx[31]: 10717, blz_base[10717]: 61450050, zweigstelle[10717]:  0, base_name[10717]: Aalen, W�rtt
 *    i: 32, start_idx[32]: 10755, blz_base[10755]: 61450191, zweigstelle[10755]:  0, base_name[10755]: Aalen, W�rtt
 *    i: 33, start_idx[33]: 10756, blz_base[10756]: 61480001, zweigstelle[10756]:  0, base_name[10756]: Aalen, W�rtt
 *    i: 34, start_idx[34]: 10757, blz_base[10757]: 61490150, zweigstelle[10757]:  0, base_name[10757]: Aalen, W�rtt
 *    i: 35, start_idx[35]: 10766, blz_base[10766]: 61490150, zweigstelle[10766]:  9, base_name[10766]: Aalen, W�rtt
 *    i: 36, start_idx[36]:  7002, blz_base[ 7002]: 51050015, zweigstelle[ 7002]: 69, base_name[ 7002]: Aarbergen
 *    i: 37, start_idx[37]:  7055, blz_base[ 7055]: 51091700, zweigstelle[ 7055]:  2, base_name[ 7055]: Aarbergen
 * 
 */

/**
 * KontoCheck::bank_suche_bic(<search_bic>)
 *
 * find all banks with a given bic
 *
 * possible return values (and short description):
 *
 *    LUT1_FILE_USED             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_BIC_NOT_INITIALIZED   "Das Feld BIC wurde nicht initialisiert"
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    KEY_NOT_FOUND              "Die Suche lieferte kein Ergebnis"
 *    OK                         "ok"
 */
static VALUE bank_suche_bic(int argc,VALUE* argv,VALUE self)
{
   char such_name[128],**base_name,error_msg[512];
   int i,j,retval,anzahl,*start_idx,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params(argc,argv,such_name,NULL,NULL,3);
   retval=lut_suche_bic(such_name,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,rb_str_new2(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * KontoCheck::bank_suche_namen(<name>)
 *
 * find all banks with a given name
 *
 * possible return values (and short description):
 *
 *    LUT1_FILE_USED             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_NAME_NOT_INITIALIZED  "Das Feld Bankname wurde nicht initialisiert"
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    KEY_NOT_FOUND              "Die Suche lieferte kein Ergebnis"
 *    OK                         "ok"
 */
static VALUE bank_suche_namen(int argc,VALUE* argv,VALUE self)
{
   char such_name[128],**base_name,error_msg[512];
   int i,j,retval,anzahl,*start_idx,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params(argc,argv,such_name,NULL,NULL,3);
   retval=lut_suche_namen(such_name,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,rb_str_new2(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * KontoCheck::bank_suche_namen_kurz(<short_name>)
 *
 * find all banks with a given short name
 *
 * possible return values (and short description):
 *
 *    LUT1_FILE_USED             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_NAME_KURZ_NOT_INITIALIZED "Das Feld Kurzname wurde nicht initialisiert"
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    KEY_NOT_FOUND              "Die Suche lieferte kein Ergebnis"
 *    OK                         "ok"
 */
static VALUE bank_suche_namen_kurz(int argc,VALUE* argv,VALUE self)
{
   char such_name[128],**base_name,error_msg[512];
   int i,j,retval,anzahl,*start_idx,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params(argc,argv,such_name,NULL,NULL,3);
   retval=lut_suche_namen_kurz(such_name,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,rb_str_new2(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * KontoCheck::bank_suche_plz(<plz1>[, plz2])
 *
 * find all banks with plz between plz1 and plz2
 *
 * possible return values (and short description):
 *
 *    LUT1_FILE_USED             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_BLZ_NOT_INITIALIZED   "Das Feld BLZ wurde nicht initialisiert"
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    KEY_NOT_FOUND              "Die Suche lieferte kein Ergebnis"
 *    OK                         "ok"
 */
static VALUE bank_suche_plz(int argc,VALUE* argv,VALUE self)
{
   char error_msg[512];
   int i,j,retval,such1,such2,anzahl,*start_idx,*base_name,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params_int(argc,argv,&such1,&such2);
   retval=lut_suche_plz(such1,such2,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,INT2FIX(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * KontoCheck::bank_suche_pz(<pz1>[, pz2])
 *
 * find all banks with check method between pz1 and pz2
 *
 * possible return values (and short description):
 *
 *    LUT1_FILE_USED             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_BLZ_NOT_INITIALIZED   "Das Feld BLZ wurde nicht initialisiert"
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    KEY_NOT_FOUND              "Die Suche lieferte kein Ergebnis"
 *    OK                         "ok"
 */
static VALUE bank_suche_pz(int argc,VALUE* argv,VALUE self)
{
   char error_msg[512];
   int i,j,retval,such1,such2,anzahl,*start_idx,*base_name,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params_int(argc,argv,&such1,&such2);
   retval=lut_suche_pz(such1,such2,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,INT2FIX(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * KontoCheck::bank_suche_blz(<blz1>[, blz2])
 *
 * find all banks with bic between blz1 and blz2
 *
 * possible return values (and short description):
 *
 *    LUT1_FILE_USED             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_BLZ_NOT_INITIALIZED   "Das Feld BLZ wurde nicht initialisiert"
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    KEY_NOT_FOUND              "Die Suche lieferte kein Ergebnis"
 *    OK                         "ok"
 */
static VALUE bank_suche_blz(int argc,VALUE* argv,VALUE self)
{
   char error_msg[512];
   int i,j,retval,such1,such2,anzahl,*start_idx,*base_name,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params_int(argc,argv,&such1,&such2);
   retval=lut_suche_blz(such1,such2,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,INT2FIX(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * KontoCheck::bank_suche_ort(<suchort>)
 *
 * find all banks in a given location
 *
 * possible return values (and short description):
 *
 *    LUT1_FILE_USED             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_ORT_NOT_INITIALIZED   "Das Feld Ort wurde nicht initialisiert"
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    KEY_NOT_FOUND              "Die Suche lieferte kein Ergebnis"
 *    OK                         "ok"
 */
static VALUE bank_suche_ort(int argc,VALUE* argv,VALUE self)
{
   char such_name[128],**base_name,error_msg[512];
   int i,j,retval,anzahl,*start_idx,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params(argc,argv,such_name,NULL,NULL,3);
   retval=lut_suche_ort(such_name,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,rb_str_new2(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * The initialization method for this module
 */
void Init_konto_check_raw()
{
   KontoCheck = rb_define_module("KontoCheckRaw");

      /* die Parameteranzahl -1 bei den folgenden Funktionen meint eine variable
       * Anzahl Parameter; die genaue Anzahl wird bei der Funktion rb_scan_args()
       * als 3. Parameter angegeben.
       *
       * Bei -1 erfolgt die �bergabe als C-Array, bei -2 ist die �bergabe als
       * Ruby-Array (s. http://karottenreibe.github.com/2009/10/28/ruby-c-extension-5/):
       *
       *    A positive number means, your function will take just that many
       *       arguments, none less, none more.
       *    -1 means the function takes a variable number of arguments, passed
       *       as a C array.
       *    -2 means the function takes a variable number of arguments, passed
       *       as a Ruby array.
       *
       * Was in ext_ruby.pdf (S.39) dazu steht, ist einfach falsch (das hat
       * mich einige Zeit gekostet):
       *
       *    In some of the function definitions that follow, the parameter argc
       *    specifies how many arguments a Ruby method takes. It may have the
       *    following values. If the value is not negative, it specifies the
       *    number of arguments the method takes. If negative, it indicates
       *    that the method takes optional arguments. In this case, the
       *    absolute value of argc minus one is the number of required
       *    arguments (so -1 means all arguments are optional, -2 means one
       *    mandatory argument followed by optional arguments, and so on).
       *
       * Es w�re zu testen, ob dieses Verhalten bei �lteren Versionen gilt;
       * dann m��te man u.U. eine entsprechende Verzweigung einbauen. Bei den
       * aktuellen Varianten (Ruby 1.8.6 und 1.9.2) funktioniert diese Variante.
       */
   rb_define_module_function(KontoCheck,"init",init,-1);
   rb_define_module_function(KontoCheck,"free",free_rb,0);
   rb_define_module_function(KontoCheck,"konto_check",konto_check,-1);
   rb_define_module_function(KontoCheck,"bank_valid",bank_valid,-1);
   rb_define_module_function(KontoCheck,"bank_filialen",bank_filialen,-1);
   rb_define_module_function(KontoCheck,"bank_alles",bank_alles,-1);
   rb_define_module_function(KontoCheck,"bank_name",bank_name,-1);
   rb_define_module_function(KontoCheck,"bank_name_kurz",bank_name_kurz,-1);
   rb_define_module_function(KontoCheck,"bank_plz",bank_plz,-1);
   rb_define_module_function(KontoCheck,"bank_ort",bank_ort,-1);
   rb_define_module_function(KontoCheck,"bank_pan",bank_pan,-1);
   rb_define_module_function(KontoCheck,"bank_bic",bank_bic,-1);
   rb_define_module_function(KontoCheck,"bank_nr",bank_nr,-1);
   rb_define_module_function(KontoCheck,"bank_pz",bank_pz,-1);
   rb_define_module_function(KontoCheck,"bank_aenderung",bank_aenderung,-1);
   rb_define_module_function(KontoCheck,"bank_loeschung",bank_loeschung,-1);
   rb_define_module_function(KontoCheck,"bank_nachfolge_blz",bank_nachfolge_blz,-1);
   rb_define_module_function(KontoCheck,"dump_lutfile",dump_lutfile_rb,-1);
   rb_define_module_function(KontoCheck,"lut_info",lut_info_rb,-1);
   rb_define_module_function(KontoCheck,"retval2txt",retval2txt_rb,1);
   rb_define_module_function(KontoCheck,"retval2dos",retval2dos_rb,1);
   rb_define_module_function(KontoCheck,"retval2html",retval2html_rb,1);
   rb_define_module_function(KontoCheck,"retval2utf8",retval2utf8_rb,1);
   rb_define_module_function(KontoCheck,"generate_lutfile",generate_lutfile_rb,-1);
   rb_define_module_function(KontoCheck,"iban_check",iban_check_rb,-1);
   rb_define_module_function(KontoCheck,"iban2bic",iban2bic_rb,-1);
   rb_define_module_function(KontoCheck,"ipi_gen",ipi_gen_rb,-1);
   rb_define_module_function(KontoCheck,"ipi_check",ipi_check_rb,-1);
   rb_define_module_function(KontoCheck,"bank_suche_bic",bank_suche_bic,-1);
   rb_define_module_function(KontoCheck,"bank_suche_namen",bank_suche_namen,-1);
   rb_define_module_function(KontoCheck,"bank_suche_namen_kurz",bank_suche_namen_kurz,-1);
   rb_define_module_function(KontoCheck,"bank_suche_ort",bank_suche_ort,-1);
   rb_define_module_function(KontoCheck,"bank_suche_blz",bank_suche_blz,-1);
   rb_define_module_function(KontoCheck,"bank_suche_plz",bank_suche_plz,-1);
   rb_define_module_function(KontoCheck,"bank_suche_pz",bank_suche_pz,-1);
   rb_define_module_function(KontoCheck,"load_bank_data",load_bank_data,1);

      /* R�ckgabewerte der konto_check Bibliothek */
   rb_define_const(KontoCheck,"KTO_CHECK_UNSUPPORTED_COMPRESSION",INT2FIX(KTO_CHECK_UNSUPPORTED_COMPRESSION));
   rb_define_const(KontoCheck,"KTO_CHECK_INVALID_COMPRESSION_LIB",INT2FIX(KTO_CHECK_INVALID_COMPRESSION_LIB));
   rb_define_const(KontoCheck,"OK_UNTERKONTO_ATTACHED",INT2FIX(OK_UNTERKONTO_ATTACHED));
   rb_define_const(KontoCheck,"KTO_CHECK_DEFAULT_BLOCK_INVALID",INT2FIX(KTO_CHECK_DEFAULT_BLOCK_INVALID));
   rb_define_const(KontoCheck,"KTO_CHECK_DEFAULT_BLOCK_FULL",INT2FIX(KTO_CHECK_DEFAULT_BLOCK_FULL));
   rb_define_const(KontoCheck,"KTO_CHECK_NO_DEFAULT_BLOCK",INT2FIX(KTO_CHECK_NO_DEFAULT_BLOCK));
   rb_define_const(KontoCheck,"KTO_CHECK_KEY_NOT_FOUND",INT2FIX(KTO_CHECK_KEY_NOT_FOUND));
   rb_define_const(KontoCheck,"LUT2_NO_LONGER_VALID_BETTER",INT2FIX(LUT2_NO_LONGER_VALID_BETTER));
   rb_define_const(KontoCheck,"DTA_SRC_KTO_DIFFERENT",INT2FIX(DTA_SRC_KTO_DIFFERENT));
   rb_define_const(KontoCheck,"DTA_SRC_BLZ_DIFFERENT",INT2FIX(DTA_SRC_BLZ_DIFFERENT));
   rb_define_const(KontoCheck,"DTA_CR_LF_IN_FILE",INT2FIX(DTA_CR_LF_IN_FILE));
   rb_define_const(KontoCheck,"DTA_INVALID_C_EXTENSION",INT2FIX(DTA_INVALID_C_EXTENSION));
   rb_define_const(KontoCheck,"DTA_FOUND_SET_A_NOT_C",INT2FIX(DTA_FOUND_SET_A_NOT_C));
   rb_define_const(KontoCheck,"DTA_FOUND_SET_E_NOT_C",INT2FIX(DTA_FOUND_SET_E_NOT_C));
   rb_define_const(KontoCheck,"DTA_FOUND_SET_C_NOT_EXTENSION",INT2FIX(DTA_FOUND_SET_C_NOT_EXTENSION));
   rb_define_const(KontoCheck,"DTA_FOUND_SET_E_NOT_EXTENSION",INT2FIX(DTA_FOUND_SET_E_NOT_EXTENSION));
   rb_define_const(KontoCheck,"DTA_INVALID_EXTENSION_COUNT",INT2FIX(DTA_INVALID_EXTENSION_COUNT));
   rb_define_const(KontoCheck,"DTA_INVALID_NUM",INT2FIX(DTA_INVALID_NUM));
   rb_define_const(KontoCheck,"DTA_INVALID_CHARS",INT2FIX(DTA_INVALID_CHARS));
   rb_define_const(KontoCheck,"DTA_CURRENCY_NOT_EURO",INT2FIX(DTA_CURRENCY_NOT_EURO));
   rb_define_const(KontoCheck,"DTA_EMPTY_AMOUNT",INT2FIX(DTA_EMPTY_AMOUNT));
   rb_define_const(KontoCheck,"DTA_INVALID_TEXT_KEY",INT2FIX(DTA_INVALID_TEXT_KEY));
   rb_define_const(KontoCheck,"DTA_EMPTY_STRING",INT2FIX(DTA_EMPTY_STRING));
   rb_define_const(KontoCheck,"DTA_MARKER_A_NOT_FOUND",INT2FIX(DTA_MARKER_A_NOT_FOUND));
   rb_define_const(KontoCheck,"DTA_MARKER_C_NOT_FOUND",INT2FIX(DTA_MARKER_C_NOT_FOUND));
   rb_define_const(KontoCheck,"DTA_MARKER_E_NOT_FOUND",INT2FIX(DTA_MARKER_E_NOT_FOUND));
   rb_define_const(KontoCheck,"DTA_INVALID_SET_C_LEN",INT2FIX(DTA_INVALID_SET_C_LEN));
   rb_define_const(KontoCheck,"DTA_INVALID_SET_LEN",INT2FIX(DTA_INVALID_SET_LEN));
   rb_define_const(KontoCheck,"DTA_WAERUNG_NOT_EURO",INT2FIX(DTA_WAERUNG_NOT_EURO));
   rb_define_const(KontoCheck,"DTA_INVALID_ISSUE_DATE",INT2FIX(DTA_INVALID_ISSUE_DATE));
   rb_define_const(KontoCheck,"DTA_INVALID_DATE",INT2FIX(DTA_INVALID_DATE));
   rb_define_const(KontoCheck,"DTA_FORMAT_ERROR",INT2FIX(DTA_FORMAT_ERROR));
   rb_define_const(KontoCheck,"DTA_FILE_WITH_ERRORS",INT2FIX(DTA_FILE_WITH_ERRORS));
   rb_define_const(KontoCheck,"INVALID_SEARCH_RANGE",INT2FIX(INVALID_SEARCH_RANGE));
   rb_define_const(KontoCheck,"KEY_NOT_FOUND",INT2FIX(KEY_NOT_FOUND));
   rb_define_const(KontoCheck,"BAV_FALSE",INT2FIX(BAV_FALSE));
   rb_define_const(KontoCheck,"LUT2_NO_USER_BLOCK",INT2FIX(LUT2_NO_USER_BLOCK));
   rb_define_const(KontoCheck,"INVALID_SET",INT2FIX(INVALID_SET));
   rb_define_const(KontoCheck,"NO_GERMAN_BIC",INT2FIX(NO_GERMAN_BIC));
   rb_define_const(KontoCheck,"IPI_CHECK_INVALID_LENGTH",INT2FIX(IPI_CHECK_INVALID_LENGTH));
   rb_define_const(KontoCheck,"IPI_INVALID_CHARACTER",INT2FIX(IPI_INVALID_CHARACTER));
   rb_define_const(KontoCheck,"IPI_INVALID_LENGTH",INT2FIX(IPI_INVALID_LENGTH));
   rb_define_const(KontoCheck,"LUT1_FILE_USED",INT2FIX(LUT1_FILE_USED));
   rb_define_const(KontoCheck,"MISSING_PARAMETER",INT2FIX(MISSING_PARAMETER));
   rb_define_const(KontoCheck,"IBAN2BIC_ONLY_GERMAN",INT2FIX(IBAN2BIC_ONLY_GERMAN));
   rb_define_const(KontoCheck,"IBAN_OK_KTO_NOT",INT2FIX(IBAN_OK_KTO_NOT));
   rb_define_const(KontoCheck,"KTO_OK_IBAN_NOT",INT2FIX(KTO_OK_IBAN_NOT));
   rb_define_const(KontoCheck,"TOO_MANY_SLOTS",INT2FIX(TOO_MANY_SLOTS));
   rb_define_const(KontoCheck,"INIT_FATAL_ERROR",INT2FIX(INIT_FATAL_ERROR));
   rb_define_const(KontoCheck,"INCREMENTAL_INIT_NEEDS_INFO",INT2FIX(INCREMENTAL_INIT_NEEDS_INFO));
   rb_define_const(KontoCheck,"INCREMENTAL_INIT_FROM_DIFFERENT_FILE",INT2FIX(INCREMENTAL_INIT_FROM_DIFFERENT_FILE));
   rb_define_const(KontoCheck,"DEBUG_ONLY_FUNCTION",INT2FIX(DEBUG_ONLY_FUNCTION));
   rb_define_const(KontoCheck,"LUT2_INVALID",INT2FIX(LUT2_INVALID));
   rb_define_const(KontoCheck,"LUT2_NOT_YET_VALID",INT2FIX(LUT2_NOT_YET_VALID));
   rb_define_const(KontoCheck,"LUT2_NO_LONGER_VALID",INT2FIX(LUT2_NO_LONGER_VALID));
   rb_define_const(KontoCheck,"LUT2_GUELTIGKEIT_SWAPPED",INT2FIX(LUT2_GUELTIGKEIT_SWAPPED));
   rb_define_const(KontoCheck,"LUT2_INVALID_GUELTIGKEIT",INT2FIX(LUT2_INVALID_GUELTIGKEIT));
   rb_define_const(KontoCheck,"LUT2_INDEX_OUT_OF_RANGE",INT2FIX(LUT2_INDEX_OUT_OF_RANGE));
   rb_define_const(KontoCheck,"LUT2_INIT_IN_PROGRESS",INT2FIX(LUT2_INIT_IN_PROGRESS));
   rb_define_const(KontoCheck,"LUT2_BLZ_NOT_INITIALIZED",INT2FIX(LUT2_BLZ_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_FILIALEN_NOT_INITIALIZED",INT2FIX(LUT2_FILIALEN_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_NAME_NOT_INITIALIZED",INT2FIX(LUT2_NAME_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_PLZ_NOT_INITIALIZED",INT2FIX(LUT2_PLZ_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_ORT_NOT_INITIALIZED",INT2FIX(LUT2_ORT_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_NAME_KURZ_NOT_INITIALIZED",INT2FIX(LUT2_NAME_KURZ_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_PAN_NOT_INITIALIZED",INT2FIX(LUT2_PAN_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_BIC_NOT_INITIALIZED",INT2FIX(LUT2_BIC_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_PZ_NOT_INITIALIZED",INT2FIX(LUT2_PZ_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_NR_NOT_INITIALIZED",INT2FIX(LUT2_NR_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_AENDERUNG_NOT_INITIALIZED",INT2FIX(LUT2_AENDERUNG_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_LOESCHUNG_NOT_INITIALIZED",INT2FIX(LUT2_LOESCHUNG_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED",INT2FIX(LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_NOT_INITIALIZED",INT2FIX(LUT2_NOT_INITIALIZED));
   rb_define_const(KontoCheck,"LUT2_FILIALEN_MISSING",INT2FIX(LUT2_FILIALEN_MISSING));
   rb_define_const(KontoCheck,"LUT2_PARTIAL_OK",INT2FIX(LUT2_PARTIAL_OK));
   rb_define_const(KontoCheck,"LUT2_Z_BUF_ERROR",INT2FIX(LUT2_Z_BUF_ERROR));
   rb_define_const(KontoCheck,"LUT2_Z_MEM_ERROR",INT2FIX(LUT2_Z_MEM_ERROR));
   rb_define_const(KontoCheck,"LUT2_Z_DATA_ERROR",INT2FIX(LUT2_Z_DATA_ERROR));
   rb_define_const(KontoCheck,"LUT2_BLOCK_NOT_IN_FILE",INT2FIX(LUT2_BLOCK_NOT_IN_FILE));
   rb_define_const(KontoCheck,"LUT2_DECOMPRESS_ERROR",INT2FIX(LUT2_DECOMPRESS_ERROR));
   rb_define_const(KontoCheck,"LUT2_COMPRESS_ERROR",INT2FIX(LUT2_COMPRESS_ERROR));
   rb_define_const(KontoCheck,"LUT2_FILE_CORRUPTED",INT2FIX(LUT2_FILE_CORRUPTED));
   rb_define_const(KontoCheck,"LUT2_NO_SLOT_FREE",INT2FIX(LUT2_NO_SLOT_FREE));
   rb_define_const(KontoCheck,"UNDEFINED_SUBMETHOD",INT2FIX(UNDEFINED_SUBMETHOD));
   rb_define_const(KontoCheck,"EXCLUDED_AT_COMPILETIME",INT2FIX(EXCLUDED_AT_COMPILETIME));
   rb_define_const(KontoCheck,"INVALID_LUT_VERSION",INT2FIX(INVALID_LUT_VERSION));
   rb_define_const(KontoCheck,"INVALID_PARAMETER_STELLE1",INT2FIX(INVALID_PARAMETER_STELLE1));
   rb_define_const(KontoCheck,"INVALID_PARAMETER_COUNT",INT2FIX(INVALID_PARAMETER_COUNT));
   rb_define_const(KontoCheck,"INVALID_PARAMETER_PRUEFZIFFER",INT2FIX(INVALID_PARAMETER_PRUEFZIFFER));
   rb_define_const(KontoCheck,"INVALID_PARAMETER_WICHTUNG",INT2FIX(INVALID_PARAMETER_WICHTUNG));
   rb_define_const(KontoCheck,"INVALID_PARAMETER_METHODE",INT2FIX(INVALID_PARAMETER_METHODE));
   rb_define_const(KontoCheck,"LIBRARY_INIT_ERROR",INT2FIX(LIBRARY_INIT_ERROR));
   rb_define_const(KontoCheck,"LUT_CRC_ERROR",INT2FIX(LUT_CRC_ERROR));
   rb_define_const(KontoCheck,"FALSE_GELOESCHT",INT2FIX(FALSE_GELOESCHT));
   rb_define_const(KontoCheck,"OK_NO_CHK_GELOESCHT",INT2FIX(OK_NO_CHK_GELOESCHT));
   rb_define_const(KontoCheck,"OK_GELOESCHT",INT2FIX(OK_GELOESCHT));
   rb_define_const(KontoCheck,"BLZ_GELOESCHT",INT2FIX(BLZ_GELOESCHT));
   rb_define_const(KontoCheck,"INVALID_BLZ_FILE",INT2FIX(INVALID_BLZ_FILE));
   rb_define_const(KontoCheck,"LIBRARY_IS_NOT_THREAD_SAFE",INT2FIX(LIBRARY_IS_NOT_THREAD_SAFE));
   rb_define_const(KontoCheck,"FATAL_ERROR",INT2FIX(FATAL_ERROR));
   rb_define_const(KontoCheck,"INVALID_KTO_LENGTH",INT2FIX(INVALID_KTO_LENGTH));
   rb_define_const(KontoCheck,"FILE_WRITE_ERROR",INT2FIX(FILE_WRITE_ERROR));
   rb_define_const(KontoCheck,"FILE_READ_ERROR",INT2FIX(FILE_READ_ERROR));
   rb_define_const(KontoCheck,"ERROR_MALLOC",INT2FIX(ERROR_MALLOC));
   rb_define_const(KontoCheck,"NO_BLZ_FILE",INT2FIX(NO_BLZ_FILE));
   rb_define_const(KontoCheck,"INVALID_LUT_FILE",INT2FIX(INVALID_LUT_FILE));
   rb_define_const(KontoCheck,"NO_LUT_FILE",INT2FIX(NO_LUT_FILE));
   rb_define_const(KontoCheck,"INVALID_BLZ_LENGTH",INT2FIX(INVALID_BLZ_LENGTH));
   rb_define_const(KontoCheck,"INVALID_BLZ",INT2FIX(INVALID_BLZ));
   rb_define_const(KontoCheck,"INVALID_KTO",INT2FIX(INVALID_KTO));
   rb_define_const(KontoCheck,"NOT_IMPLEMENTED",INT2FIX(NOT_IMPLEMENTED));
   rb_define_const(KontoCheck,"NOT_DEFINED",INT2FIX(NOT_DEFINED));
   rb_define_const(KontoCheck,"FALSE",INT2FIX(FALSE));
   rb_define_const(KontoCheck,"OK",INT2FIX(OK));
   rb_define_const(KontoCheck,"OK_NO_CHK",INT2FIX(OK_NO_CHK));
   rb_define_const(KontoCheck,"OK_TEST_BLZ_USED",INT2FIX(OK_TEST_BLZ_USED));
   rb_define_const(KontoCheck,"LUT2_VALID",INT2FIX(LUT2_VALID));
   rb_define_const(KontoCheck,"LUT2_NO_VALID_DATE",INT2FIX(LUT2_NO_VALID_DATE));
   rb_define_const(KontoCheck,"LUT1_SET_LOADED",INT2FIX(LUT1_SET_LOADED));
   rb_define_const(KontoCheck,"LUT1_FILE_GENERATED",INT2FIX(LUT1_FILE_GENERATED));
   rb_define_const(KontoCheck,"DTA_FILE_WITH_WARNINGS",INT2FIX(DTA_FILE_WITH_WARNINGS));
   rb_define_const(KontoCheck,"LUT_V2_FILE_GENERATED",INT2FIX(LUT_V2_FILE_GENERATED));
   rb_define_const(KontoCheck,"KTO_CHECK_VALUE_REPLACED",INT2FIX(KTO_CHECK_VALUE_REPLACED));
   rb_define_const(KontoCheck,"OK_UNTERKONTO_POSSIBLE",INT2FIX(OK_UNTERKONTO_POSSIBLE));
   rb_define_const(KontoCheck,"OK_UNTERKONTO_GIVEN",INT2FIX(OK_UNTERKONTO_GIVEN));
   rb_define_const(KontoCheck,"OK_SLOT_CNT_MIN_USED",INT2FIX(OK_SLOT_CNT_MIN_USED));
}
