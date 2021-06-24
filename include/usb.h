#ifndef UNFL_USB_H
#define UNFL_USB_H
    
    /*********************************
             DataType macros
    *********************************/
    
    // UNCOMMENT THE #DEFINE IF USING LIBDRAGON
    #define LIBDRAGON                    

    // Settings
    #define USE_OSRAW          0           // Use if you're doing USB operations without the PI Manager (libultra only)
    #define DEBUG_ADDRESS_SIZE 8*1024*1024 // Max size of USB I/O. The bigger this value, the more ROM you lose!
   
    // Cart definitions
    #define CART_NONE      0
    #define CART_64DRIVE   1
    #define CART_EVERDRIVE 2
    #define CART_SC64      3
    
    // Data types defintions
    #define DATATYPE_TEXT       0x01
    #define DATATYPE_RAWBINARY  0x02
    #define DATATYPE_HEADER     0x03
    #define DATATYPE_SCREENSHOT 0x04
    
    extern int usb_datatype;
    extern int usb_datasize;
    extern int usb_dataleft;
    extern int usb_readblock;

    
    /*********************************
            Convenience macros
    *********************************/
    
    // Use these to conveniently read the header from usb_poll()
    #define USBHEADER_GETTYPE(header) ((header & 0xFF000000) >> 24)
    #define USBHEADER_GETSIZE(header) ((header & 0x00FFFFFF))
    

    /*********************************
              USB Functions
    *********************************/

    /*==============================
        usb_initialize
        Initializes the USB buffers and pointers
        @return 1 if the USB initialization was successful, 0 if not
    ==============================*/
    
    extern char usb_initialize();
    
    
    /*==============================
        usb_getcart
        Returns which flashcart is currently connected
        @return The CART macro that corresponds to the identified flashcart
    ==============================*/
    
    extern char usb_getcart();
    
    
    /*==============================
        usb_write
        Writes data to the USB.
        Will not write if there is data to read from USB
        @param The DATATYPE that is being sent
        @param A buffer with the data to send
        @param The size of the data being sent
    ==============================*/
    
    extern void usb_write(int datatype, const void* data, int size);
    
    
    /*==============================
        usb_poll
        Returns the header of data being received via USB
        The first byte contains the data type, the next 3 the number of bytes left to read
        @return The data header, or 0
    ==============================*/
    
    extern unsigned long usb_poll();
    
    
    /*==============================
        usb_read
        Reads bytes from USB into the provided buffer
        @param The buffer to put the read data in
        @param The number of bytes to read
    ==============================*/
    
    extern void usb_read(void* buffer, int size);
    
    
    /*==============================
        usb_skip
        Skips a USB read by the specified amount of bytes
        @param The number of bytes to skip
    ==============================*/
    
    extern void usb_skip(int nbytes);
    
    
    /*==============================
        usb_rewind
        Rewinds a USB read by the specified amount of bytes
        @param The number of bytes to rewind
    ==============================*/
    
    extern void usb_rewind(int nbytes);
    
    
    /*==============================
        usb_purge
        Purges the incoming USB data
    ==============================*/
    
    extern void usb_purge();

#endif