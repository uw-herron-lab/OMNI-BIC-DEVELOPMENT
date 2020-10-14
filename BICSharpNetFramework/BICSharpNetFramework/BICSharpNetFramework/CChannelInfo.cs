using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace BICSharpNetFramework
{
    public class CChannelInfo
    {
        // Constructor
        /// <summary>
        /// 
        /// </summary>
        /// <param name="canMeasure"></param>
        /// <param name="measureValueMin"></param>
        /// <param name="measureValueMax"></param>
        /// <param name="canStimulate"></param>
        /// <param name="stimulationUnit"></param>
        /// <param name="stimValueMin"></param>
        /// <param name="stimValueMax"></param>
        /// <param name="canMeasureImpedance"></param>
        public CChannelInfo(bool canMeasure, double measureValueMin, double measureValueMax, bool canStimulate, UnitType stimulationUnit, double stimValueMin, double stimValueMax, bool canMeasureImpedance)
        {
            // Assign 
            m_canMeasure = canMeasure;
            m_measureValueMin = measureValueMin;
            m_measureValueMax = measureValueMax;
            m_canStimulate = canStimulate;
            m_stimulationUnit = stimulationUnit;
            m_stimValueMin = stimValueMin;
            m_stimValueMax = stimValueMax;
            m_canMeasureImpedance = canMeasureImpedance;
        }

        // Public accessors
        /// <summary>
        /// 
        /// </summary>
        public virtual bool canMeasure 
        {
            get { return m_canMeasure; }
        }

        /// <summary>
        /// 
        /// </summary>
        public virtual double getMeasureValueMin
        {
            get { return m_measureValueMin; }
        }

        /// <summary>
        /// 
        /// </summary>
        public virtual double getMeasureValueMax
        {
            get { return m_measureValueMax; }
        }

        /// <summary>
        /// 
        /// </summary>
        public virtual bool canStimulate
        {
            get { return m_canStimulate; }
        }

        /// <summary>
        /// 
        /// </summary>
        public virtual UnitType getStimulationUnit
        {
            get { return m_stimulationUnit; }
        }
        
        /// <summary>
        /// 
        /// </summary>
        public virtual double getStimValueMin
        {
            get { return m_stimValueMin; }
        }

        /// <summary>
        /// 
        /// </summary>
        public virtual double getStimValueMax
        {
            get { return m_stimValueMax; }
        }

        /// <summary>
        /// 
        /// </summary>
        public virtual bool canMeasureImpedance
        {
            get { return m_canMeasureImpedance; }
        }

        // Private member variables
        private bool m_canMeasure;
        private double m_measureValueMin;
        private double m_measureValueMax;
        private bool m_canStimulate;
        private UnitType m_stimulationUnit;
        private double m_stimValueMin;
        private double m_stimValueMax;
        private bool m_canMeasureImpedance;
    }
}
