const App = () => {
  const [view, setView] = React.useState('single'); // or 'multi'
  
  return (
    <div>
      <nav className="bg-gray-800 text-white p-4">
        <div className="max-w-7xl mx-auto flex gap-4">
          <button 
            onClick={() => setView('single')}
            className={`px-4 py-2 rounded ${
              view === 'single' 
                ? 'bg-blue-500' 
                : 'hover:bg-gray-700'
            }`}
          >
            Single Lock View
          </button>
          <button 
            onClick={() => setView('multi')}
            className={`px-4 py-2 rounded ${
              view === 'multi' 
                ? 'bg-blue-500' 
                : 'hover:bg-gray-700'
            }`}
          >
            Multi Lock View
          </button>
        </div>
      </nav>
      
      {view === 'single' ? <LockTimeline /> : <MultiLockTimeline />}
    </div>
  );
};

ReactDOM.render(<App />, document.getElementById('root'));