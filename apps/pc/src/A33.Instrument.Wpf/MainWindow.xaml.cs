using System.Windows;

namespace A33.Instrument.Wpf;

public partial class MainWindow : Window
{
    private readonly MainViewModel viewModel=new();
    public MainWindow(){InitializeComponent();DataContext=viewModel;Closed+=OnClosed;}
    private async void OnClosed(object? sender,EventArgs e)=>await viewModel.DisposeAsync();
}
