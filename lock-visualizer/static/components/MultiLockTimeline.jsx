window.MultiLockTimeline = () => {
    const [availableLocks, setAvailableLocks] = React.useState([]);
    const [selectedLocks, setSelectedLocks] = React.useState(new Set());
    const [timelineData, setTimelineData] = React.useState(null);
    const [mouseY, setMouseY] = React.useState(null);
    const [lockColors, setLockColors] = React.useState({});
    
    const timelineRef = React.useRef(null);
    
    // Generate HSL color with golden ratio for good distribution
    const generateColors = (numColors) => {
        const colors = {};
        const selectedArray = Array.from(selectedLocks);
        
        const hueStep = 360 / (numColors * 1.618033988749895);  // golden ratio
        
        selectedArray.forEach((lock, index) => {
            const hue = (hueStep * index) % 360;
            colors[lock] = {
                // Define colors as style objects instead of Tailwind classes
                bgNormal: `hsl(${hue}, 85%, 95%)`,
                bgHover: `hsl(${hue}, 85%, 90%)`,
                border: `hsl(${hue}, 60%, 60%)`,
                text: `hsl(${hue}, 80%, 35%)`
            };
        });
        
        return colors;
    };
    
    React.useEffect(() => {
        setLockColors(generateColors(selectedLocks.size));
    }, [selectedLocks]);
    
    React.useEffect(() => {
        fetch('/api/locks')
            .then(res => res.json())
            .then(data => setAvailableLocks(data.locks));
    }, []);
    
    React.useEffect(() => {
        if (selectedLocks.size > 0) {
            fetch('/api/multi_timeline', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ locks: Array.from(selectedLocks) })
            })
                .then(res => res.json())
                .then(data => {
                    console.log('Timeline data:', data);
                    setTimelineData(data);
                });
        } else {
            setTimelineData(null);
        }
    }, [selectedLocks]);
    
    const handleLockToggle = (lockAddr) => {
        const newSelected = new Set(selectedLocks);
        if (newSelected.has(lockAddr)) {
            newSelected.delete(lockAddr);
        } else {
            newSelected.add(lockAddr);
        }
        setSelectedLocks(newSelected);
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

    const formatNumber = num => {
        if (num === undefined || num === null) return 'N/A';
        return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    };
    
    if (!timelineData || selectedLocks.size === 0) {
        return (
            <div className="min-h-screen bg-gray-50 p-8">
                <div className="max-w-7xl mx-auto">
                    <h1 className="text-3xl font-bold text-gray-900 mb-8">Multi-Lock Timeline</h1>
                    
                    <div className="bg-white rounded-lg shadow-lg p-6 mb-8">
                        <div className="grid grid-cols-4 gap-4 mb-6">
                            {availableLocks.map(lock => {
                                const isSelected = selectedLocks.has(lock);
                                const colors = lockColors[lock];
                                return (
                                    <button
                                        key={lock}
                                        onClick={() => handleLockToggle(lock)}
                                        style={isSelected && colors ? {
                                            backgroundColor: colors.bgNormal,
                                            borderColor: colors.border,
                                            color: colors.text
                                        } : {}}
                                        className={`
                                            p-3 rounded-lg border-2 transition-all duration-200
                                            ${isSelected 
                                                ? 'font-medium' 
                                                : 'border-gray-200 hover:border-gray-300 text-gray-600'}
                                        `}
                                    >
                                        Lock 0x{lock.toString(16)}
                                    </button>
                                );
                            })}
                        </div>
                        
                        {selectedLocks.size === 0 && (
                            <div className="text-center text-gray-500 py-8">
                                Select locks to view their timeline
                            </div>
                        )}
                    </div>
                </div>
            </div>
        );
    }
    
    const timelineStart = Math.min(...timelineData.events.map(e => e.timestamp));
    const timelineEnd = Math.max(...timelineData.events.map(e => 
        e.timestamp + (e.duration || 0)
    ));
    const timelineDuration = timelineEnd - timelineStart;
    
    const padding = timelineDuration * 0.02;
    const paddedStart = timelineStart - padding;
    const paddedEnd = timelineEnd + padding;
    
    const timeToY = time => ((time - paddedStart) / (paddedEnd - paddedStart)) * 2000;
    const yToTime = y => (y / 2000) * (paddedEnd - paddedStart) + paddedStart;
    
    return (
        <div className="min-h-screen bg-gray-50 p-8">
            <div className="max-w-7xl mx-auto">
                <h1 className="text-3xl font-bold text-gray-900 mb-8">Multi-Lock Timeline</h1>
                
                <div className="bg-white rounded-lg shadow-lg p-6 mb-8">
                    <div className="grid grid-cols-4 gap-4 mb-6">
                        {availableLocks.map(lock => {
                            const isSelected = selectedLocks.has(lock);
                            const colors = lockColors[lock];
                            return (
                                <button
                                    key={lock}
                                    onClick={() => handleLockToggle(lock)}
                                    className={`
                                        p-3 rounded-lg border-2 transition-all duration-200
                                        ${isSelected && colors
                                            ? `${colors.bg} ${colors.border} ${colors.text} font-medium` 
                                            : 'border-gray-200 hover:border-gray-300 text-gray-600'}
                                    `}
                                >
                                    Lock 0x{lock.toString(16)}
                                </button>
                            );
                        })}
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
                                const time = paddedStart + ((paddedEnd - paddedStart) / 20) * i;
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
                                    const colors = lockColors[event.lock_addr] || {
                                        bgNormal: '#f9fafb',
                                        border: '#d1d5db',
                                        text: '#1f2937'
                                    };
                                    return (
                                        <div
                                            key={i}
                                            className={`
                                                absolute rounded-sm cursor-pointer transform hover:scale-105 hover:z-10
                                                transition-all duration-200 overflow-hidden border
                                            `}
                                            style={{
                                                backgroundColor: colors.bgNormal,
                                                borderColor: colors.border,
                                                top: `${timeToY(event.timestamp)}px`,
                                                left: '4px',
                                                width: '112px',
                                                height: `${timeToY(event.timestamp + event.duration) - timeToY(event.timestamp)}px`,
                                            }}
                                        >
                                            <div 
                                                className="p-1 text-xs font-medium" 
                                                style={{ color: colors.text }}
                                            >
                                                <div>0x{event.lock_addr.toString(16)}</div>
                                                <div className="font-mono">{formatNumber(event.duration)}</div>
                                            </div>
                                        </div>
                                    );
                                })}
                            </div>
                        ))}
                    </div>
                </div>
            </div>
        </div>
    );
};