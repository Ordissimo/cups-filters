/*
 * "$Id: cupsaddsmb.c,v 1.3.2.1 2001/12/26 16:52:56 mike Exp $"
 *
 *   "cupsaddsmb" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2001 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()             - Export printers on the command-line.
 *   do_samba_command() - Do a SAMBA command, asking for a password as needed.
 *   export_dest()      - Export a destination to SAMBA.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/cups.h>
#include <cups/string.h>
#include <errno.h>


/*
 * Local globals...
 */

int	Verbosity = 0;


/*
 * Local functions...
 */

int	do_samba_command(const char *, const char *, const char *);
int	export_dest(const char *);


/*
 * 'main()' - Export printers on the command-line.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int	i, j;		/* Looping vars */
  int	status;		/* Status from export_dest() */
  int	num_printers;	/* Number of printers */
  char	**printers;	/* Printers */


  for (i = 1; i < argc; i ++)
    if (strcmp(argv[i], "-a") == 0)
    {
      num_printers = cupsGetPrinters(&printers);

      for (j = 0, status = 0; j < num_printers; j ++)
        if ((status = export_dest(printers[j])) != 0)
	  break;

      for (j = 0; j < num_printers; j ++)
        free(printers[j]);

      if (num_printers)
        free(printers);

      if (status)
        return (status);
    }
    else if (strcmp(argv[i], "-U") == 0)
    {
      i ++;
      if (i >= argc)
      {
	puts("Usage: cupsaddsmb [-a] [-U user] [-v] [printer1 ... printerN]");
	return (1);
      }

      cupsSetUser(argv[i]);
    }
    else if (strcmp(argv[i], "-v") == 0)
      Verbosity = 1;
    else if (argv[i][0] != '-')
    {
      if ((status = export_dest(argv[i])) != 0)
	return (status);
    }
    else
    {
      puts("Usage: cupsaddsmb [-a] [-U user] [-v] [printer1 ... printerN]");
      return (1);
    }

  return (0);
}


/*
 * 'do_samba_command()' - Do a SAMBA command, asking for
 *                        a password as needed.
 */

int					/* O - Status of command */
do_samba_command(const char *command,	/* I - Command to run */
                 const char *args,	/* I - Argument(s) for command */
                 const char *subcmd)	/* I - Sub-command */
{
  int		status;			/* Status of command */
  char		temp[4096];		/* Command/prompt string */
  static const char *p = NULL;		/* Password data */


  for (status = 1;;)
  {
    if (p)
      snprintf(temp, sizeof(temp), "%s -N -U\'%s%%%s\' %s -c \'%s\'",
               command, cupsUser(), p, args, subcmd);
    else
      snprintf(temp, sizeof(temp), "%s -N -U\'%s\' %s -c \'%s\'",
               command, cupsUser(), args, subcmd);

    if (Verbosity)
      printf("Running command: %s\n", temp);
    else
    {
      strncat(temp, " </dev/null >/dev/null 2>/dev/null", sizeof(temp) - 1);
      temp[sizeof(temp) - 1] = '\0';
    }

    if (Verbosity)
      printf("Running the following command:\n\n    %s\n", temp);
    else
    {
      strncat(temp, " >/dev/null 2>/dev/null", sizeof(temp) - 1);
      temp[sizeof(temp) - 1] = '\0';
    }

    if ((status = system(temp)) != 0)
    {
      if (Verbosity)
        puts("");

      snprintf(temp, sizeof(temp),
               "Password for %s required to access %s via SAMBA: ",
	       cupsUser(), cupsServer());

      if ((p = cupsGetPassword(temp)) == NULL)
        break;
    }
    else
    {
      if (Verbosity)
        puts("");

      break;
    }
  }

  return (status);
}


/*
 * 'export_dest()' - Export a destination to SAMBA.
 */

int				/* O - 0 on success, non-zero on error */
export_dest(const char *dest)	/* I - Destination to export */
{
  int		status;		/* Status of smbclient/rpcclient commands */
  const char	*ppdfile;	/* PPD file for printer drivers */
  char		command[1024],	/* Command to run */
		subcmd[1024];	/* Sub-command */
  const char	*datadir;	/* CUPS_DATADIR */


  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  /* Get the PPD file... */
  if ((ppdfile = cupsGetPPD(dest)) == NULL)
  {
    fprintf(stderr, "Warning: No PPD file for printer \"%s\"!\n", dest);
    return (1);
  }

  /* Do the smbclient commands needed for the Windows drivers... */
  snprintf(command, sizeof(command), "smbclient //%s/print\\$", cupsServer());

  snprintf(subcmd, sizeof(subcmd),
           "mkdir W32X86;"
	   "put %s W32X86/%s.PPD;"
	   "put %s/drivers/ADOBEPS5.DLL W32X86/ADOBEPS5.DLL;"
	   "put %s/drivers/ADOBEPSU.DLL W32X86/ADOBEPSU.DLL;"
	   "put %s/drivers/ADOBEPSU.HLP W32X86/ADOBEPSU.HLP",
	   ppdfile, dest, datadir, datadir, datadir);

  if ((status = do_samba_command(command, "", subcmd)) != 0)
  {
    fprintf(stderr, "ERROR: Unable to copy Windows printer driver files (%d)!\n",
            status);
    unlink(ppdfile);
    return (3);
  }

  snprintf(subcmd, sizeof(subcmd),
           "mkdir WIN40;"
	   "put %s WIN40/%s.PPD;"
	   "put %s/drivers/ADFONTS.MFM WIN40/ADFONTS.MFM;"
	   "put %s/drivers/ADOBEPS4.DRV WIN40/ADOBEPS4.DRV;"
	   "put %s/drivers/ADOBEPS4.HLP WIN40/ADOBEPS4.HLP;"
	   "put %s/drivers/DEFPRTR2.PPD WIN40/DEFPRTR2.PPD;"
	   "put %s/drivers/ICONLIB.DLL WIN40/ICONLIB.DLL;"
	   "put %s/drivers/PSMON.DLL WIN40/PSMON.DLL;",
	   ppdfile, dest, datadir, datadir, datadir,
	   datadir, datadir, datadir);

  if ((status = do_samba_command(command, "", subcmd)) != 0)
  {
    fprintf(stderr, "ERROR: Unable to copy Windows printer driver files (%d)!\n",
            status);
    unlink(ppdfile);
    return (3);
  }

  unlink(ppdfile);

  /* Do the rpcclient commands needed for the Windows drivers... */
  snprintf(subcmd, sizeof(subcmd),
           "adddriver \"Windows NT x86\" \"%s:ADOBEPS5.DLL:%s.PPD:ADOBEPSU.DLL:ADOBEPSU.HLP:NULL:RAW:NULL\"",
	   dest, dest);

  if ((status = do_samba_command("rpcclient", cupsServer(), subcmd)) != 0)
  {
    fprintf(stderr, "ERROR: Unable to install Windows printer driver files (%d)!\n",
            status);
    return (5);
  }

  snprintf(subcmd, sizeof(subcmd), "addprinter %s %s \"%s\" \"\"",
	   dest, dest, dest);

  if ((status = do_samba_command("rpcclient", cupsServer(), subcmd)) != 0)
  {
    fprintf(stderr, "ERROR: Unable to install Windows printer driver files (%d)!\n",
            status);
    return (5);
  }

  snprintf(subcmd, sizeof(subcmd),
	   "adddriver \"Windows 4.0\" \"%s:ADOBEPS4.DRV:%s.PPD:NULL:ADOBEPS4.HLP:PSMON.DLL:RAW:ADFONTS.MFM,DEFPRTR2.PPD,ICONLIB.DLL\"",
	   dest, dest);

  if ((status = do_samba_command("rpcclient", cupsServer(), subcmd)) != 0)
  {
    fprintf(stderr, "ERROR: Unable to install Windows printer driver files (%d)!\n",
            status);
    return (5);
  }

  return (0);
}


/*
 * End of "$Id: cupsaddsmb.c,v 1.3.2.1 2001/12/26 16:52:56 mike Exp $".
 */
