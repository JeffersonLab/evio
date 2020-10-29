package org.jlab.coda.jevio;

import java.util.EventListener;

/**
 * In SAX like behavior, implementors will listen for structures encountered when an event is parsed.
 * 
 * @author heddle
 * 
 */
public interface IEvioListener extends EventListener {

	/**
	 * Called after a structure is read in while parsing or searching an event
	 * and any filter has accepted it.
	 * 
	 * NOTE: the user should NOT modify the arguments.
	 * 
	 * @param topStructure the evio structure at the top of the search/parse
	 * @param structure the full structure, including header
	 */
	void gotStructure(BaseStructure topStructure, IEvioStructure structure);
    
    /**
     * Starting to parse a new event structure.
     * @param structure the event structure in question.
     */
    void startEventParse(BaseStructure structure);

    /**
     * Done parsing a new event structure.
     * @param structure the event structure in question.
     */
    void endEventParse(BaseStructure structure);

}
