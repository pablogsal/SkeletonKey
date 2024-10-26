window.LockTimeline = () => {
    const [locks, setLocks] = React.useState([]);
    const [selectedLock, setSelectedLock] = React.useState(null);
    const [timelineData, setTimelineData] = React.useState(null);
    const [mouseY, setMouseY] = React.useState(null);
    
    const timelineRef = React.useRef(null);
    
    React.useEffect(() => {
        fetch('/api/locks')
            .then(res => res.json())
            .then(data => {
                console.log('Locks data:', data);
                setLocks(data.locks);
                if (data.locks.length > 0) {
                    setSelectedLock(data.locks[0]);
                }
            });
    }, []);
    
    React.useEffect(() => {
        if (selectedLock) {
            fetch(`/api/timeline/${selectedLock}`)
                .then(res => res.json())
                .then(data => {
                    console.log('Timeline data:', data);
                    setTimelineData(data);
                });
        }
    }, [selectedLock]);
    
    if (!timelineData) return <div>Loading...</div>;
    
    const rawStart = Math.min(...timelineData.events.map(e => e.timestamp));
    const rawEnd = Math.max(...timelineData.events.map(e => 
        e.timestamp + (e.duration || 0)
    ));
    const timelineDuration = rawEnd - rawStart;
    
    const padding = timelineDuration * 0.02;
    const timelineStart = rawStart - padding;
    const timelineEnd = rawEnd + padding;
    const paddedDuration = timelineEnd - timelineStart;
    
    const timeToY = time => ((time - timelineStart) / paddedDuration) * 2000;
    const yToTime = y => (y / 2000) * paddedDuration + timelineStart;
    
    const formatNumber = num => {
        if (num === undefined || num === null) return 'N/A';
        return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    };
    
    const handleMouseMove = (e) => {
        if (!timelineRef.current) return;
        const rect = timelineRef.current.getBoundingClientRect();
        const y = Math.max(0, Math.min(2000, e.clientY - rect.top));
        setMouseY(y);
    };
    
    const handleMouseLeave = () => {
        setMouseY(null);
    };
    
    return (
        <div className="min-h-screen bg-gray-50 p-8">
            <div className="max-w-7xl mx-auto">
                <h1 className="text-3xl font-bold text-gray-900 mb-8">Lock Timeline</h1>
                
                <div className="bg-white rounded-lg shadow-lg p-6 mb-8">
                    <div className="flex items-center justify-between mb-6">
                        <label className="text-sm font-semibold text-gray-700">Select Lock:</label>
                        <select
                            className="px-4 py-2 border border-gray-300 rounded-md shadow-sm focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                            value={selectedLock || ''}
                            onChange={e => setSelectedLock(Number(e.target.value))}
                        >
                            {locks.map(lock => (
                                <option key={lock} value={lock}>
                                    Lock 0x{lock.toString(16)}
                                </option>
                            ))}
                        </select>
                    </div>
                    
                    <div 
                        ref={timelineRef}
                        className="relative select-none cursor-crosshair" 
                        style={{ height: '2000px', marginLeft: '120px' }}
                        onMouseMove={handleMouseMove}
                        onMouseLeave={handleMouseLeave}
                    >
                        {mouseY !== null && (
                            <>
                                <div 
                                    className="absolute left-0 right-0 border-t-2 border-blue-400 pointer-events-none z-30"
                                    style={{ top: `${mouseY}px` }}
                                />
                                <div 
                                    className="absolute right-0 bg-blue-500 text-white px-2 py-1 rounded text-xs font-mono transform -translate-y-1/2 z-30"
                                    style={{ top: `${mouseY}px`, marginRight: '8px' }}
                                >
                                    Time: {formatNumber(yToTime(mouseY))}
                                </div>
                            </>
                        )}

                        <div className="absolute left-0 top-0 bottom-0" style={{ width: '120px', marginLeft: '-120px' }}>
                            {Array.from({ length: 21 }).map((_, i) => {
                                const time = timelineStart + (paddedDuration / 20) * i;
                                return (
                                    <div
                                        key={i}
                                        className="absolute left-0 right-0 flex items-center justify-end pr-2 group"
                                        style={{ top: `${timeToY(time)}px` }}
                                    >
                                        <span className="text-xs text-gray-500 whitespace-nowrap bg-white px-1 rounded">
                                            {formatNumber(time)}
                                        </span>
                                        <div className="absolute left-0 right-0 border-t border-gray-100" 
                                             style={{ width: '2000px', marginLeft: '120px' }} />
                                    </div>
                                );
                            })}
                        </div>

                        {/* Thread headers */}
                        <div className="absolute top-0 left-0 right-0 h-12 flex border-b border-gray-200 bg-white/90 backdrop-blur-sm z-20">
                            {timelineData.threads.map((thread, index) => (
                                <div
                                    key={thread}
                                    className="flex-none text-sm font-mono py-3 px-4 font-semibold text-gray-700"
                                    style={{ width: '120px' }}
                                >
                                    Thread {thread}
                                </div>
                            ))}
                        </div>

                        {/* Events */}
                        {timelineData.threads.map((thread, threadIndex) => (
                            <div
                                key={thread}
                                className="absolute top-0 bottom-0 border-l border-gray-200"
                                style={{ left: `${threadIndex * 120}px`, width: '120px' }}
                            >
                                {timelineData.events
                                    .filter(event => event.tid === thread)
                                    .map((event, i) => {
                                        const style = {
                                            position: 'absolute',
                                            top: `${timeToY(event.timestamp)}px`,
                                            left: '4px',
                                            width: '112px',
                                            height: event.duration ? `${timeToY(event.timestamp + event.duration) - timeToY(event.timestamp)}px` : '2px'
                                        };

                                        if (event.type === 'wait') {
                                            return (
                                                <div
                                                    key={i}
                                                    style={style}
                                                    className="border border-yellow-400 bg-yellow-50 hover:bg-yellow-100 rounded-sm"
                                                >
                                                    <div className="p-1 text-xs font-medium text-yellow-800">
                                                        <div>Wait</div>
                                                        <div className="font-mono">{formatNumber(event.duration)}</div>
                                                    </div>
                                                </div>
                                            );
                                        }
                                        
                                        if (event.type === 'held') {
                                            return (
                                                <div
                                                    key={i}
                                                    style={style}
                                                    className="border border-green-400 bg-green-50 hover:bg-green-100 rounded-sm"
                                                >
                                                    <div className="p-1 text-xs font-medium text-green-800">
                                                        <div>Held</div>
                                                        <div className="font-mono">{formatNumber(event.duration)}</div>
                                                    </div>
                                                </div>
                                            );
                                        }
                                        
                                        return null;
                                    })}
                            </div>
                        ))}
                    </div>
                </div>
            </div>
        </div>
    );
};