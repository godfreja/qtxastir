/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id: dbfawk.c,v 1.19 2004/05/26 21:14:27 we7u Exp $
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2003-2004  The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 *
 */
/*
 * This library glues the Awk-like functions (see awk.c) to attributes
 * for shapefiles (contained in DBF files).
 *
 * Alan Crosswell, n2ygk@weca.org
 *
 */


//
// Functions which allocate memory:
// --------------------------------
// dbfawk_field_list
// dbfawk_load_sigs
// dbfawk_find_sig
// dbfawk_parse_record (indirectly)
//
// Functions which free memory:
// ----------------------------
// dbfawk_free_info
// dbfawk_load_sigs
// dbfawk_free_sig
// dbfawk_free_sigs
// dbfawk_find_sig
//


#include "config.h"
#if defined(WITH_DBFAWK) && defined(HAVE_LIBSHP) && defined(HAVE_LIBPCRE)
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include "awk.h"
#include "dbfawk.h"





/*
 * dbfawk_sig:  Generate a signature for a DBF file.
 *  Fills in sig and returns number of fields.
 */
int dbfawk_sig(DBFHandle dbf, char *sig, int size) {
  int nf = 0;

//fprintf(stderr,"dbfawk_sig start\n");
  if (sig && size > 0 && dbf) {
    int i;
    char *sp;
    int width,prec;
    
    nf = DBFGetFieldCount(dbf);
    for (i = 0, sp=sig; sp < &sig[size-XBASE_FLDHDR_SZ] && i < nf ; i++) {
      DBFGetFieldInfo(dbf,i,sp,&width,&prec);
      sp += strlen(sp);
      *sp++ = ':';              /* field name separator */
    }
    if (i)
      *--sp = '\0';             /* clobber the trailing sep */
  }
//fprintf(stderr,"dbfawk_sig stop\n");
 
  return nf;
}





/* Free a field list */
void dbfawk_free_info ( dbfawk_field_info *list) {
    dbfawk_field_info *x, *p;

//fprintf(stderr,"dbfawk_free_info start\n");
//fprintf(stderr,"dbfawk_free_info: free\n");
 
    for ( p = list; p != NULL; ) {
        x = p;
        p = p->next;
        free(x);
//WE7U
// Free's memory
//fprintf(stderr,"f1\n");
    }

//fprintf(stderr,"dbfawk_free_info stop\n");
 
}





/*
 * dbfawk_field_list: Generate a list of info about fields to read for
 *  a given DBFHandle and colon-separated list of fieldnames.
 */
dbfawk_field_info *dbfawk_field_list(DBFHandle dbf, char *dbffields) {
  dbfawk_field_info *fi = NULL, *head = NULL, *prev;
  int nf;
  char *sp;

//fprintf(stderr,"dbfawk_field_info start\n");
//fprintf(stderr,"dbfawk_field_list: calloc\n");
 
  /* now build up the list of fields to read */
  for (nf = 0, sp = dbffields, prev = NULL; *sp; nf++) {
    char *d,*p = sp;
    char junk[XBASE_FLDHDR_SZ];
    int w,prec;

    fi = calloc(1,sizeof(dbfawk_field_info));
    if (!fi) {
        fprintf(stderr,"dbfawk_field_list: first calloc failed\n");
        // Free whatever list we have at this point?
        if (head) {
            dbfawk_free_info(head);
        }
//fprintf(stderr,"dbfawk_field_info stop\n");
 
        return NULL;
    }

//WE7U
// Allocates memory
//fprintf(stderr,"a1\n");    
 
    if (prev) {
        prev->next = fi;
    } else {                    /* no prev, must be first one */
        head = fi;
    }
    d = fi->name;
    while (*p && *p != ':') *d++ = *p++;
    if (*p == ':')
      *d = *p++ = '\0';
    fi->num = DBFGetFieldIndex(dbf, fi->name);
    fi->type = DBFGetFieldInfo(dbf, fi->num, junk, &w, &prec);
    sp = p;
    prev = fi;
  }
  if (!head)
    fprintf(stderr,"dbfawk_field_list: head is NULL\n");

//fprintf(stderr,"dbfawk_field_info stop\n");
 
  return head;
}





/*
 * dbfawk_load_sigs:  Load up dbfawk signature mappings
 *  Reads *.dbfawk and registers dbffields "signature".
 *  Returns head of sig_info list.
 *
 *  TODO - consider whether it makes sense to use a private symtbl,
 *   compile and then free here or require the caller to pass in a
 *   symtbl that has dbfinfo declared.
 */
// Malloc's dbfawk_sig_info and returns a filled-in list

dbfawk_sig_info *dbfawk_load_sigs(const char *dir, /* directory path */
                                  const char *ftype) /* filetype */ {
    DIR *d;
    struct dirent *e;
    int ftlen;
    dbfawk_sig_info *i = NULL, *head = NULL;
    awk_symtab *symtbl;
    char dbfinfo[1024];         /* local copy of signature */

//fprintf(stderr,"dbfawk_load_sigs start\n");
//fprintf(stderr,"dbfawk_load_sigs: calloc/strdup\n");
 
 
    if (!dir || !ftype) {
//fprintf(stderr,"dbfawk_load_sigs stop\n");
 
        return NULL;
    }
    ftlen = strlen(ftype);
    d = opendir(dir);
    if (!d) {
//fprintf(stderr,"dbfawk_load_sigs stop\n");
 
        return NULL;
    }

//WE7U2
// Allocates new memory, free'd before each return, so we're ok with
// this memory allocation.
    symtbl = awk_new_symtab();
    if (!symtbl) {
//fprintf(stderr,"dbfawk_load_sigs stop\n");
 
        return NULL;
    }

//WE7U
// Allocates new memory, free'd before each return.
    awk_declare_sym(symtbl,"dbfinfo",STRING,dbfinfo,sizeof(dbfinfo));

    while ((e = readdir(d)) != NULL) {
        int len = strlen(e->d_name);
//WE7U
// Allocates memory, free'd later
        char *path = calloc(1,len+strlen(dir)+2);
 
        if (!path) {
//WE7U2
// Free's memory before return
            if (symtbl)
            awk_free_symtab(symtbl);
 
            fprintf(stderr,"failed to malloc in dbfawk.c!\n");
//fprintf(stderr,"dbfawk_load_sigs stop\n");
 
            return NULL;
        }
//WE7U
//fprintf(stderr,"a2\n");    
 
        *dbfinfo = '\0';
        if (len > ftlen && (strcmp(&e->d_name[len-ftlen],ftype) == 0)) {
            if (!head) {
                i = head = calloc(1,sizeof(dbfawk_sig_info));
//WE7U
// Allocates memory
//fprintf(stderr,"a3\n");    
            } else {
                i->next = calloc(1,sizeof(dbfawk_sig_info));
//WE7U
// Allocates memory
//fprintf(stderr,"a4\n");    
                i = i->next;
            }
            strcpy(path,dir);
            strcat(path,"/");
            strcat(path,e->d_name);
//WE7U2
// Calls awk_new_program/awk_new_rule which allocate new memory
            i->prog = awk_load_program_file(path);

//WE7U2
// Calls awk_compile_action which allocates new memory
            if (awk_compile_program(symtbl,i->prog) < 0) {
                fprintf(stderr,"%s: failed to parse\n",e->d_name);
            } else {
                /* dbfinfo must be defined in BEGIN rule */
                awk_exec_begin(i->prog); 

//WE7U2
// strdup allocates memory
                i->sig = strdup(dbfinfo);

//WE7U2
// Does some free's
                awk_uncompile_program(i->prog);
            }
        }
        free(path);
//WE7U
// Free's memory
//fprintf(stderr,"f2\n");

    }

//WE7U2
// Free's memory
    if (symtbl)
        awk_free_symtab(symtbl);

//fprintf(stderr,"dbfawk_load_sigs stop\n");
 
    return head;
}





void dbfawk_free_sig(dbfawk_sig_info *ptr) {

//fprintf(stderr,"dbfawk_free_sig start\n");
//fprintf(stderr,"dbfawk_free_sig: free\n");
 
    if (ptr) {
        if (ptr->prog)
//WE7U2
// Free's memory
            awk_free_program(ptr->prog);

        if (ptr->sig) {
            free(ptr->sig);
//WE7U
// Free's memory
//fprintf(stderr,"f3\n");
        }
        free(ptr);
    }
//fprintf(stderr,"dbfawk_free_sig stop\n");
 
}





void dbfawk_free_sigs(dbfawk_sig_info *list) {
    dbfawk_sig_info *x, *p;

//fprintf(stderr,"dbfawk_free_sigs start\n");
 
    for (p = list; p; ) {
        x = p;
        p = p->next;
//WE7U
// Free's memory
        dbfawk_free_sig(x);
    }
//fprintf(stderr,"dbfawk_free_sigs stop\n");
 
}





/*
 * dbfawk_find_sig:  Given a DBF file's "signature", find the appropriate
 * awk program.  If filename is not null, see if there's a per-file .dbfawk
 * and load it.
 */

dbfawk_sig_info *dbfawk_find_sig(dbfawk_sig_info *Dbf_sigs,
                                 const char *sig,
                                 const char *file) {
    dbfawk_sig_info *result = NULL;

//fprintf(stderr,"dbfawk_find_sig start\n");
//fprintf(stderr,"dbfawk_find_sig: calloc\n");

    if (file) {
        char *dot, *perfile = calloc(1,strlen(file)+7);
        dbfawk_sig_info *info;

        if (!perfile) {
            fprintf(stderr,"failed to malloc in dbfawk_find_sig!\n");
//fprintf(stderr,"dbfawk_find_sig stop\n");
 
            return NULL;
        }
//WE7U
// Allocates memory
//fprintf(stderr,"a5\n");


        strcpy(perfile,file);
        dot = strrchr(perfile,'.');
        if (dot)
            *dot = '\0';
        strcat(perfile,".dbfawk");
        info = calloc(1,sizeof(*info));
 
        if (!info) {
            fprintf(stderr,"failed to malloc in dbfawk_find_sig!\n");
            free(perfile);
//fprintf(stderr,"dbfawk_find_sig stop\n");
 
            return NULL;
        }
//WE7U
// Allocates memory
//fprintf(stderr,"a6\n");
 
//WE7U2
// Calls awk_new_program/awk_new_rule which allocate new memory
        info->prog = awk_load_program_file(perfile);
        /* N.B. info->sig is left NULL since it won't be searched, and 
           to flag that it's safe to free this memory when we're done with
           it */
        info->sig = NULL;
        free(perfile);
//WE7U
// Free's memory
//fprintf(stderr,"f4\n");
        if (info->prog) {
//fprintf(stderr,"dbfawk_find_sig stop\n");
 
            return info;
        }
        else {
            dbfawk_free_sigs(info);
//WE7U
// Free's memory
//fprintf(stderr,"f5\n");
        }
        /* fall through and do normal signature search */
    }

    for (result = Dbf_sigs; result; result = result->next) {
        if (strcmp(result->sig,sig) == 0) {
//fprintf(stderr,"dbfawk_find_sig stop\n");
 
            return result;
        }
    }
//fprintf(stderr,"dbfawk_find_sig stop\n");
 
    return NULL;
}





/*
 * dbfawk_parse_record:  Read a dbf record and parse only the fields
 *  listed in 'fi' using the program, 'rs'.
 */
void dbfawk_parse_record(awk_program *rs,
                         DBFHandle dbf,
                         dbfawk_field_info *fi,
                         int i) {
    dbfawk_field_info *finfo;

//fprintf(stderr,"dbfawk_parse_record start\n");
 
    awk_exec_begin_record(rs); /* execute a BEGIN_RECORD rule if any */

    for (finfo = fi; finfo ; finfo = finfo->next) {
        char qbuf[1024];

        switch (finfo->type) {
        case FTString:
	    sprintf(qbuf,"%s=%s",finfo->name,DBFReadStringAttribute(dbf,i,finfo->num));
	    break;
        case FTInteger:
	    sprintf(qbuf,"%s=%d",finfo->name,DBFReadIntegerAttribute(dbf,i,finfo->num));
	    break;
        case FTDouble:
	    sprintf(qbuf,"%s=%f",finfo->name,DBFReadDoubleAttribute(dbf,i,finfo->num));
	    break;
        case FTInvalid:
        default:
	    sprintf(qbuf,"%s=??",finfo->name);
	    break;
        }
        if (awk_exec_program(rs,qbuf,strlen(qbuf)) == 2)
            break;
    }
    awk_exec_end_record(rs); /* execute an END_RECORD rule if any */

//fprintf(stderr,"dbfawk_parse_record stop\n");
 
}





#ifndef HAVE_DBFGETFIELDINDEX
#include <ctype.h>
/************************************************************************/
/*                            str_to_upper()                            */
/************************************************************************/

static void str_to_upper (char *string) {
  int len;
  short i = -1;

  len = strlen (string);

  while (++i < len)
    if (isalpha(string[i]) && islower(string[i]))
      string[i] = toupper ((int)string[i]);
}





/************************************************************************/
/*                          DBFGetFieldIndex()                          */
/*                                                                      */
/*      Get the index number for a field in a .dbf file.                */
/*                                                                      */
/*      Contributed by Jim Matthews.                                    */
/************************************************************************/

int DBFGetFieldIndex(DBFHandle psDBF, const char *pszFieldName) {
  char          name[12], name1[12], name2[12];
  int           i;

  strncpy(name1, pszFieldName,11);
  str_to_upper(name1);

  for( i = 0; i < DBFGetFieldCount(psDBF); i++ ) {
      DBFGetFieldInfo( psDBF, i, name, NULL, NULL );
      strncpy(name2,name,11);
      str_to_upper(name2);

      if(!strncmp(name1,name2,10))
	return(i);
    }
  return(-1);
}



#endif
#endif /* HAVE_LIBSHP && HAVE_LIBPCRE */


