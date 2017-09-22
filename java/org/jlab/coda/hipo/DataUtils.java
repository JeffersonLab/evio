/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.nio.ByteBuffer;

/**
 *
 * @author gavalian
 */
public class DataUtils {
    public DataUtils(){
        
    }
    
    public static String getHexStringInt(int value){
        StringBuilder str = new StringBuilder();
        String sv= Integer.toHexString(value);
        int nzeros = 8 - sv.length();
        for ( int i = 0; i < nzeros; i++) str.append("0");
        str.append(sv);
        return str.toString();
    }
    /**
     * 
     * @param buffer
     * @param wrap
     * @return 
     */
    public static String getStringArray(ByteBuffer buffer, int wrap, int max){
        StringBuilder str = new StringBuilder();
        int limit = buffer.limit();
        int counter = 1;
        for(int i = 0; i < limit; i+=4){
            int value = buffer.getInt(i);
            str.append(String.format("%10s", DataUtils.getHexStringInt(value)));
            if( (counter)%wrap==0) str.append("\n");
            counter++;
            if(counter>max) break;
        }
        return str.toString();
    }
}
