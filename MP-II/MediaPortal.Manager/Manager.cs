#region Copyright (C) 2007-2008 Team MediaPortal

/*
 *  Copyright (C) 2007-2008 Team MediaPortal
 *  http://www.team-mediaportal.com
 *
 *  This file is part of MediaPortal II
 *
 *  MediaPortal II is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  MediaPortal II is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MediaPortal II.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#endregion

using System;
using System.Collections.Generic;
using System.Windows.Forms;

using MediaPortal.Utilities.CommandLine;

using MediaPortal;
using MediaPortal.Core;
using MediaPortal.Core.PathManager;
using MediaPortal.Core.Threading;
using MediaPortal.Core.Localisation;
using MediaPortal.Core.Logging;
using MediaPortal.Core.PluginManager;
using MediaPortal.Core.Settings;

using MediaPortal.Services.PathManager;
using MediaPortal.Services.Threading;
using MediaPortal.Services.Localisation;
using MediaPortal.Services.Logging;
using MediaPortal.Services.PluginManager;
using MediaPortal.Services.Settings;


namespace MediaPortal.Manager
{
  static class Manager
  {
    /// <summary>
    /// The main entry point for the application.
    /// </summary>
    [STAThread]
    static void Main(params string[] args)
    {
      System.Threading.Thread.CurrentThread.Name = "Manager";
      Application.EnableVisualStyles();
      Application.SetCompatibleTextRenderingDefault(false);

      // Parse Command Line options
      CommandLineOptions mpArgs = new CommandLineOptions();
      try
      {
        CommandLine.Parse(args, mpArgs);
      }
      catch (ArgumentException)
      {
        mpArgs.DisplayOptions();
        return;
      }

      using (new ServiceScope(true)) //This is the first servicescope
      {
        // Create PathManager
        PathManager pathManager = new PathManager();

        // Check if user wants to override the default Application Data location.
        if (mpArgs.IsOption(CommandLineOptions.Option.Data))
          pathManager.ReplacePath("DATA", (string)mpArgs.GetOption(CommandLineOptions.Option.Data));

        // Register PathManager
        ServiceScope.Add<IPathManager>(pathManager);

        //Check whether the user wants to log method names in the logger
        //This adds an extra 10 to 40 milliseconds to the log call, depending on the length of the stack trace
        bool logMethods = mpArgs.IsOption(CommandLineOptions.Option.LogMethods);
        LogLevel level = LogLevel.All;
        if (mpArgs.IsOption(CommandLineOptions.Option.LogLevel))
        {
          level = (LogLevel)mpArgs.GetOption(CommandLineOptions.Option.LogLevel);
        }
        ILogger logger = new FileLogger(pathManager.GetPath(@"<LOG>\Manager.log"), level, logMethods);
        ServiceScope.Add(logger);

        logger.Debug("MPApplication: Registering Plugin Manager");
        ServiceScope.Add<IPluginManager>(new PluginManager());

        logger.Debug("MPApplication: Registering Settings Manager");
        ServiceScope.Add<ISettingsManager>(new SettingsManager());

        logger.Debug("MPApplication: Registering Strings Manager");
        ServiceScope.Add<ILocalisation>(new StringManager());

#if !DEBUG
        // Not in Debug mode (ie Release) then catch all Exceptions
        // In Debug mode these will be left unhandled.
      try
      {
#endif
        Application.Run(new MainWindow());
#if !DEBUG
        }
        catch (Exception ex)
        {
          CrashLogger crash = new CrashLogger(pathManager.GetPath("<LOG>"));
          crash.CreateLog(ex);
          //Form frm =
          //  new YesNoDialogScreen("MediaPortal II", "Unrecoverable Error",
          //                        "MediaPortal has encountered an unrecoverable error\r\nDetails have been logged\r\n\r\nRestart?",
          //                        BaseScreen.Image.bug);
          //restart = frm.ShowDialog() == DialogResult.Yes;
        }
#endif
      }
    }
  }
}