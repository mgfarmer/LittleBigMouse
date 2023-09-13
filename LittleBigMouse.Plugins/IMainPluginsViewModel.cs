﻿using System.Windows.Input;
using HLab.Mvvm.Annotations;
using LittleBigMouse.DisplayLayout.Monitors;

namespace LittleBigMouse.Plugins;

public interface IMainPluginsViewModel
{
    void AddButton(IUiCommand cmd);

    Type ContentViewMode { get; set; }

    object? Content { get; set; }

    void SetMonitorFrameViewMode<T>() where T : ViewMode
    {
        ContentViewMode = typeof(T);
    }

}