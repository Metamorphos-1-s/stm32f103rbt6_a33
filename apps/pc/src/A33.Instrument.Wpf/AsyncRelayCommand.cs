using System.Windows.Input;

namespace A33.Instrument.Wpf;

public sealed class AsyncRelayCommand(Func<Task> execute, Func<bool>? canExecute = null) : ICommand
{
    private bool running;
    public bool CanExecute(object? parameter) => !running && (canExecute?.Invoke() ?? true);
    public event EventHandler? CanExecuteChanged;
    public async void Execute(object? parameter){if(!CanExecute(parameter))return;running=true;RaiseCanExecuteChanged();try{await execute();}finally{running=false;RaiseCanExecuteChanged();}}
    public void RaiseCanExecuteChanged()=>CanExecuteChanged?.Invoke(this,EventArgs.Empty);
}
