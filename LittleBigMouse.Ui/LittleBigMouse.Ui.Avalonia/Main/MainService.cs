﻿/*
  LittleBigMouse.Control.Core
  Copyright (c) 2021 Mathieu GRENET.  All right reserved.

  This file is part of LittleBigMouse.Control.Core.

    LittleBigMouse.Control.Core is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    LittleBigMouse.Control.Core is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MouseControl.  If not, see <http://www.gnu.org/licenses/>.

	  mailto:mathieu@mgth.fr
	  http://www.mgth.fr
*/

using System;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Threading;
using HLab.Base.Avalonia;
using HLab.Mvvm;
using HLab.Mvvm.Annotations;
using HLab.Mvvm.Avalonia;
using HLab.Sys.Windows.Monitors;
using HLab.UserNotification;
using LittleBigMouse.DisplayLayout;
using LittleBigMouse.DisplayLayout.Monitors;
using LittleBigMouse.Plugins;
using LittleBigMouse.Ui.Avalonia.Persistency;
using LittleBigMouse.Ui.Avalonia.Updater;
using LittleBigMouse.Zoning;
using Live.Avalonia;
using ReactiveUI;

namespace LittleBigMouse.Ui.Avalonia.Main;

public class LiveView(Func<Window, object> windowFactory) : ILiveView
{
    public object CreateView(Window window) => windowFactory(window);
}

public class MainService : ReactiveModel, IMainService
{
    readonly IMvvmService _mvvmService;
    readonly ISystemMonitorsService _monitors;

    readonly IUserNotificationService _notify;
    readonly ILittleBigMouseClientService _littleBigMouseClientService;

    Action<IMainPluginsViewModel>? _actions;

    readonly Func<IMainPluginsViewModel> _mainViewModelLocator;

    public IMonitorsLayout MonitorsLayout 
    { 
        get => _monitorLayout; 
        set => this.RaiseAndSetIfChanged(ref _monitorLayout, value);
    }
    private IMonitorsLayout _monitorLayout;

    public MainService
    (
        Func<IMainPluginsViewModel> mainViewModelLocator,
        IMvvmService mvvmService,
        ILittleBigMouseClientService littleBigMouseClientService,
        IUserNotificationService notify,
        ISystemMonitorsService monitors
    )
    {
        _notify = notify;
        _monitors = monitors;
        _littleBigMouseClientService = littleBigMouseClientService;

        _mvvmService = mvvmService;
        _mainViewModelLocator = mainViewModelLocator;

        // Relate service state with notify icon
        _littleBigMouseClientService.DaemonEventReceived += (o, a) => DaemonEventReceived(o, a);
    }

    public void UpdateLayout()
    {
        // TODO : move to plugin
        if (_monitors is not SystemMonitorsService monitors) return;

        monitors.UpdateDevices();

        var old = MonitorsLayout;
        old?.Dispose();

        MonitorsLayout = new MonitorsLayout().UpdateFrom(monitors);

        //Dispatcher.UIThread.RunJobs(DispatcherPriority.Normal);

        _mainWindow?.UpdateLayout();
    }

    Window _mainWindow = null!;

    public async Task ShowControlAsync()
    {
        if (_mainWindow?.IsLoaded == true)
        {
            _mainWindow.WindowState = WindowState.Normal;
            _mainWindow.Activate();
            return;
        }

        var viewModel = _mainViewModelLocator();
        viewModel.MainService = this;

        _actions?.Invoke(viewModel);

        var view = await _mvvmService
            .MainContext
            .GetViewAsync<DefaultViewMode>(viewModel, typeof(IDefaultViewClass));

        _mainWindow = view?.AsWindow() ?? throw new Exception("No window found");

        _mainWindow.Closed += (s, a) => _mainWindow = null!;

        _mainWindow.Show();

        // TODO : memory test
        Dispatcher.UIThread.RunJobs();
        GC.Collect();
    }


    public async Task StartNotifierAsync()
    {
        _notify.Click += async (s, a) => await ShowControlAsync();

        await _notify.AddMenuAsync(-1, "Check for update","Icon/lbm_on", CheckUpdateAsync);
        await _notify.AddMenuAsync(-1, "Open","Icon/lbm_off", ShowControlAsync);
        await _notify.AddMenuAsync(-1, "Start","Icon/Start", StartAsync);
        await _notify.AddMenuAsync(-1, "Stop","Icon/Stop", () =>
        {
            MonitorsLayout.Enabled = false; 
            MonitorsLayout.SaveEnabled();
            return _littleBigMouseClientService.StopAsync();
        });
        await _notify.AddMenuAsync(-1, "Exit", "Icon/sys/Close", QuitAsync);

        await _notify.SetIconAsync("Icon/lbm_off",128);

        _notify.Show();

        if(MonitorsLayout.AutoUpdate)
            await CheckUpdateBlindAsync();
    }

    public void AddControlPlugin(Action<IMainPluginsViewModel>? action)
    {
        _actions += action;
    }

    async Task QuitAsync()
    {
        // TODO : it should not append by sometimes the QuitAsync does not return
        var t = Task.Delay(5000).ContinueWith(t => Dispatcher.UIThread.BeginInvokeShutdown(DispatcherPriority.Normal));
        await _littleBigMouseClientService.QuitAsync();
        Dispatcher.UIThread.BeginInvokeShutdown(DispatcherPriority.Normal);
    }

    Task StartAsync() => _littleBigMouseClientService.StartAsync(MonitorsLayout.ComputeZones());

    bool _justConnected = false;
    async Task DaemonEventReceived(object? sender, LittleBigMouseServiceEventArgs args)
    {
        switch (args.Event)
        {
            case LittleBigMouseEvent.Running:
                _justConnected = false;
                await _notify.SetIconAsync("icon/lbm_on",32);
                break;

            case LittleBigMouseEvent.Stopped:
                await _notify.SetIconAsync("icon/lbm_off",32);

                if (MonitorsLayout is not null && MonitorsLayout.Enabled && _justConnected)
                {
                    _justConnected = false;
                    await StartAsync();
                }
                break;

            case LittleBigMouseEvent.Dead:
                await _notify.SetIconAsync("icon/lbm_dead",32);
                break;

            case LittleBigMouseEvent.Paused:
                await _notify.SetIconAsync("icon/lbm_paused",32);
                break;

            case LittleBigMouseEvent.DisplayChanged:
                await DisplayChangedAsync();
                break;

            case LittleBigMouseEvent.DesktopChanged:
            case LittleBigMouseEvent.FocusChanged:
                break;
            case LittleBigMouseEvent.Connected:
                _justConnected = true;
                break;
            default:
                throw new ArgumentOutOfRangeException(nameof(args.Event), args.Event, null);
        }
    }

    private async Task DisplayChangedAsync()
    {
        UpdateLayout();
        if(MonitorsLayout.Enabled)
        {
            await StartAsync();
        }
    }

    async Task CheckUpdateAsync()
    {
        var updater = new ApplicationUpdaterViewModel();

        var updaterView = new ApplicationUpdaterView
        {
            DataContext = updater
        };
        updaterView.Show();

        await updater.CheckVersion();

        if (updater.NewVersionFound)
        {

            if (updater.Updated)
            {
                //Application.Current.Shutdown();
                return;
            }
        }
    }

    async Task CheckUpdateBlindAsync()
    {
        var updater = new ApplicationUpdaterViewModel();

        await updater.CheckVersion();

        if (updater.NewVersionFound)
        {
            var updaterView = new ApplicationUpdaterView
            {
                DataContext = updater
            };
            updaterView.Show();

            if (updater.Updated)
            {
                //Application.Current.Shutdown();
                return;
            }
        }
    }

}