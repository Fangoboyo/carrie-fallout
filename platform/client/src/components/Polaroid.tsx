import React from 'react';
import { motion } from 'framer-motion';
import { cn } from '@/lib/utils';

interface PolaroidProps {
  children: React.ReactNode;
  className?: string;
  tapeColor?: 'pink' | 'blue' | 'yellow' | 'none';
  rotation?: number;
  delay?: number;
  noTape?: boolean;
}

export function Polaroid({ 
  children, 
  className, 
  tapeColor = 'none', 
  rotation = 0,
  delay = 0,
  noTape = false
}: PolaroidProps) {
  
  const tapeColors = {
    pink: "bg-[#f2b8c6] rotate-3",
    blue: "bg-[#a2c8e3] -rotate-1",
    yellow: "bg-[#f8e1a8] -rotate-2",
    none: ""
  };

  return (
    <motion.div
      initial={{ opacity: 0, y: 50, rotate: 0 }}
      animate={{ opacity: 1, y: 0, rotate: rotation }}
      transition={{ 
        type: "spring", 
        stiffness: 260, 
        damping: 20, 
        delay: delay 
      }}
      whileHover={{ scale: 1.02, rotate: 0, zIndex: 10 }}
      className={cn(
        "relative bg-white p-6 pb-12 shadow-[0_4px_15px_rgba(0,0,0,0.08)] border border-gray-100",
        className
      )}
    >
      {!noTape && tapeColor !== 'none' && (
        <div className={cn(
          "absolute -top-3 left-1/2 -translate-x-1/2 w-24 h-6 opacity-85 shadow-[0_1px_3px_rgba(0,0,0,0.1)] z-10",
          tapeColors[tapeColor]
        )} />
      )}
      {children}
    </motion.div>
  );
}
