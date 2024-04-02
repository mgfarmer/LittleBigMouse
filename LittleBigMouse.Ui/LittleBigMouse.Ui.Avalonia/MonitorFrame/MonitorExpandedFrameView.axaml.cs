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

using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using HLab.Mvvm.Annotations;
using LittleBigMouse.Plugins;
using LittleBigMouse.Plugins.Avalonia;

namespace LittleBigMouse.Ui.Avalonia.MonitorFrame;

public partial class MonitorExpandedFrameView : UserControl, IView<ListViewMode, MonitorFrameViewModel>, IMonitorFrameView 
{
    public MonitorExpandedFrameView() => InitializeComponent();

    protected override void OnLoaded(RoutedEventArgs e)
    {
        base.OnLoaded(e);
        if(Design.IsDesignMode) return;

        var parent = this.FindAncestorOfType<IMonitorsLayoutPresenterView>();

        if(DataContext is IMonitorFrameViewModel viewModel) 
            viewModel.MonitorsPresenter = parent?.DataContext as IMonitorsLayoutPresenterViewModel;
    }


    void ResetSize_Click(object sender, RoutedEventArgs e) {
        if (DataContext is IMonitorFrameViewModel vm)
        {
            // TODO Avalonia
            // vm.Model?.Model.InitSize(vm.Model.ActiveSource.Source.Device);
        }
    }

    public IMonitorFrameViewModel? ViewModel => DataContext as IMonitorFrameViewModel;
}


