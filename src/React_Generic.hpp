/****************************************************************************************************************************
  React_Generic.hpp
  
  React_Generic is a library for ESP32, ESP8266, Protenta_H7, STM32F7, etc.
  
  Based on and modified from :
  
  1) Reactduino   (https://github.com/Reactduino/Reactduino)
  2) ReactESP     (https://github.com/mairas/ReactESP)
  
  Built by Khoi Hoang https://github.com/khoih-prog/React_Generic
 
  Version: 2.1.0
  
  Version Modified By   Date      Comments
  ------- -----------  ---------- -----------
  2.1.0    K Hoang     23/03/2022 Initial porting and coding to support ESP32, ESP8266, RP2040, STM32, nRF52, Teensy 4.x
 *****************************************************************************************************************************/

#pragma once

#ifndef REACT_GENERIC_HPP_
#define REACT_GENERIC_HPP_

#define REACT_GENERIC_VERSION            "React_Generic v2.1.0"

#define REACT_GENERIC_VERSION_MAJOR      2
#define REACT_GENERIC_VERSION_MINOR      1
#define REACT_GENERIC_VERSION_PATCH      0

#define REACT_GENERIC_VERSION_INT        2001000

#include <Arduino.h>

#include <forward_list>
#include <functional>
#include <queue>

#include "React_Generic_Debug.h"

namespace react_generic 
{
  #if defined(ESP32) || defined(ESP8266)
  typedef std::function<void()> react_callback;
  #else
  typedef void (*react_callback)(void);
  #endif
  typedef void (*isr_react_callback)(void*);
  
  // forward declarations
  
  class React_Generic;
  
  // ESP32 doesn't have the micros64 function defined
  #ifdef ESP32
    uint64_t ICACHE_RAM_ATTR micros64();
  #elif !defined(ESP8266)
    uint64_t micros64();
  #endif
  
  /**
     @brief Reactions are code to be called when a given condition is fulfilled
  */
  class Reaction 
  {
    protected:
      const react_callback callback;
  
    public:
      /**
         @brief Construct a new Reaction object
  
         @param callback Function to be called when the reaction is triggered
      */
      Reaction(react_callback callback) : callback(callback) {}
      // FIXME: why do these have to be defined?
      virtual void add(React_Generic* app = nullptr) = 0;
      virtual void remove(React_Generic* app = nullptr) = 0;
      virtual void tick() = 0;
  };
  
  /**
     @brief TimedReactions are called based on elapsing of time.
  */
  class TimedReaction : public Reaction 
  {
    protected:
      const uint64_t interval;
      uint64_t last_trigger_time;
      bool enabled;
      // A repeat reaction needs to know which app it belongs to
      React_Generic* app_context = nullptr;
  
    public:
      /**
         @brief Construct a new Timed Reaction object
  
         @param interval Interval or delay for the reaction, in milliseconds
         @param callback Function to be called when the reaction is triggered
      */
      TimedReaction(const uint32_t interval, const react_callback callback)
        : Reaction(callback), interval((uint64_t)1000 * (uint64_t)interval) 
      {
        last_trigger_time = micros64();
        enabled = true;
      }
      /**
         @brief Construct a new Timed Reaction object
  
         @param interval Interval, in microseconds
         @param callback Function to be called when the reaction is triggered
      */
      TimedReaction(const uint64_t interval, const react_callback callback)
        : Reaction(callback), interval(interval) 
      {
        last_trigger_time = micros64();
        enabled = true;
      }
  
      virtual ~TimedReaction() {}
      
      bool operator<(const TimedReaction& other);
      void add(React_Generic* app = nullptr) override;
      void remove(React_Generic* app = nullptr) override;
      
      uint32_t getTriggerTime() 
      {
        return (last_trigger_time + interval) / 1000;
      }
      
      uint64_t getTriggerTimeMicros() 
      {
        return (last_trigger_time + interval);
      }
      
      bool isEnabled() 
      {
        return enabled;
      }
      
      virtual void tick() = 0;
  };
  
  struct TriggerTimeCompare 
  {
    bool operator()(TimedReaction* a, TimedReaction* b) 
    {
      return *b < *a;
    }
  };
  
  /**
     @brief Reaction that is triggered after a certain time delay
  */
  class DelayReaction : public TimedReaction 
  {
    public:
      /**
         @brief Construct a new Delay Reaction object
  
         @param delay Delay, in milliseconds
         @param callback Function to be called after the delay
      */
      DelayReaction(const uint32_t delay, const react_callback callback);
      /**
         @brief Construct a new Delay Reaction object
  
         @param delay Delay, in microseconds
         @param callback Function to be called after the delay
      */
      DelayReaction(const uint64_t delay, const react_callback callback);
      
      virtual ~DelayReaction() {}
      
      void tick();
  };
  
  /**
     @brief Reaction that is triggered repeatedly
  */
  class RepeatReaction : public TimedReaction 
  {
    public:
      /**
         @brief Construct a new Repeat Reaction object
  
         @param interval Repetition interval, in milliseconds
         @param callback Function to be called at every repetition
      */
      RepeatReaction(const uint32_t interval, const react_callback callback)
        : TimedReaction(interval, callback) {}
      /**
         @brief Construct a new Repeat Reaction object
  
         @param interval Repetition interval, in microseconds
         @param callback Function to be called at every repetition
      */
      RepeatReaction(const uint64_t interval, const react_callback callback)
        : TimedReaction(interval, callback) {}
        
      void tick();
  };
  
  /**
     @brief Reactions that are triggered based on something else than time
  */
  class UntimedReaction : public Reaction 
  {
    public:
      UntimedReaction(const react_callback callback) : Reaction(callback) {}
      virtual ~UntimedReaction() {}
      
      virtual void add(React_Generic* app = nullptr) override;
      virtual void remove(React_Generic* app = nullptr) override;
      virtual void tick() = 0;
  };
  
  /**
     @brief Reaction that is triggered when there is input available at the given
       Arduino Stream
  */
  class StreamReaction : public UntimedReaction 
  {
    private:
      Stream& stream;
  
    public:
      /**
         @brief Construct a new Stream Reaction object
  
         @param stream Stream to monitor
         @param callback Callback to call for new input
      */
      StreamReaction(Stream& stream, const react_callback callback)
        : UntimedReaction(callback), stream(stream) {}
      void tick();
  };
  
  /**
     @brief Reaction that is triggered unconditionally at each execution loop
  */
  class TickReaction : public UntimedReaction 
  {
    public:
      /**
         @brief Construct a new Tick Reaction object
  
         @param callback Function to be called at each execution loop
      */
      TickReaction(const react_callback callback) : UntimedReaction(callback) {}
      
      void tick();
  };
  
  /**
     @brief Reaction that is triggered on an input pin change
  */
  class ISRReaction : public Reaction 
  {
    private:
  #if defined(ESP32) || defined(ESP8266)
      const uint8_t pin_number;
      const int mode;
  #else
      const pin_size_t pin_number;
      const PinStatus mode;
  #endif
  #ifdef ESP32
      // set to true once gpio_install_isr_service is called
      static bool isr_service_installed;
      static void isr(void* arg);
  #endif
  
    public:
      /**
         @brief Construct a new ISRReaction object
  
         @param pin_number GPIO pin number to which the interrupt is attached
         @param mode Interrupt mode. One of RISING, FALLING, CHANGE
         @param callback Interrupt callback. Keep this function short and add the
         ICACHE_RAM_ATTR attribute.
      */
  #if defined(ESP32) || defined(ESP8266)
      ISRReaction(uint8_t pin_number, int mode, const react_callback callback)
  #else
      ISRReaction(pin_size_t pin_number, PinStatus mode, const react_callback callback)
  #endif
        : Reaction(callback), pin_number(pin_number), mode(mode) 
      {
  #ifdef ESP32
        gpio_int_type_t intr_type;
        
        switch (mode) 
        {
          case RISING:
            intr_type = GPIO_INTR_POSEDGE;
            break;
          case FALLING:
            intr_type = GPIO_INTR_NEGEDGE;
            break;
          case CHANGE:
            intr_type = GPIO_INTR_ANYEDGE;
            break;
          default:
            intr_type = GPIO_INTR_DISABLE;
            break;
        }
        
        // configure the IO pin
        gpio_set_intr_type((gpio_num_t)pin_number, intr_type);
  
        if (!isr_service_installed) 
        {
          gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
          isr_service_installed = true;
        }
  #endif
      }
      
      virtual ~ISRReaction() {}
      void add(React_Generic* app = nullptr) override;
      void remove(React_Generic* app = nullptr) override;
      void tick() {}
  };
  
  ///////////////////////////////////////
  // React_Generic main class implementation
  
  /**
     @brief Main class of a React_Generic program
  */
  class React_Generic 
  {
      friend class Reaction;
      friend class TimedReaction;
      friend class RepeatReaction;
      friend class UntimedReaction;
      friend class ISRReaction;
  
    public:
      /**
         @brief Construct a new React_Generic object.
  
         @param singleton If true, set the singleton instance to this object
      */
      React_Generic(bool singleton = true) 
      {
        if (singleton) 
          app = this;
      }
      
      void tick(void);
  
      /// Static singleton reference to the instantiated React_Generic object
      static React_Generic* app;
  
      /**
         @brief Create a new DelayReaction
  
         @param t Delay, in milliseconds
         @param cb Callback function
         @return DelayReaction
      */
      DelayReaction* onDelay(const uint32_t t, const react_callback cb);
      /**
         @brief Create a new DelayReaction
  
         @param t Delay, in microseconds
         @param cb Callback function
         @return DelayReaction
      */
      DelayReaction* onDelayMicros(const uint64_t t, const react_callback cb);
      /**
         @brief Create a new RepeatReaction
  
         @param t Interval, in milliseconds
         @param cb Callback function
         @return RepeatReaction
      */
      RepeatReaction* onRepeat(const uint32_t t, const react_callback cb);
      /**
         @brief Create a new RepeatReaction
  
         @param t Interval, in microseconds
         @param cb Callback function
         @return RepeatReaction
      */
      RepeatReaction* onRepeatMicros(const uint64_t t, const react_callback cb);
      /**
         @brief Create a new StreamReaction
  
         @param stream Arduino Stream object to monitor
         @param cb Callback function
         @return StreamReaction
      */
      StreamReaction* onAvailable(Stream& stream, const react_callback cb);
      /**
         @brief Create a new ISRReaction (interrupt reaction)
  
         @param pin_number GPIO pin number
         @param mode One of CHANGE, RISING, FALLING
         @param cb Interrupt handler to call. This should be a very simple function,
         ideally setting a flag variable or incrementing a counter. The function
         should be defined with ICACHE_RAM_ATTR.
         @return ISRReaction
      */
  #if defined(ESP32) || defined(ESP8266)
      ISRReaction* onInterrupt(const uint8_t pin_number, int mode,
                               const react_callback cb);
  #else
      ISRReaction* onInterrupt(const pin_size_t pin_number, PinStatus mode,
                               const react_callback cb);
  #endif
      /**
         @brief Create a new TickReaction
  
         @param cb Callback function to be called at every loop execution
         @return TickReaction
      */
      TickReaction* onTick(const react_callback cb);
  
      /**
         @brief Remove a reaction from the list of active reactions
  
         @param reaction Reaction to remove
      */
      void remove(Reaction* reaction);
  
    private:
    
      std::priority_queue<TimedReaction*, std::vector<TimedReaction*>, TriggerTimeCompare> timed_queue;
      std::forward_list<UntimedReaction*> untimed_list;
      std::forward_list<ISRReaction*> isr_reaction_list;
      std::forward_list<ISRReaction*> isr_pending_list;
      
      void tickTimed();
      void tickUntimed();
      void tickISR();
      void add(Reaction* re);
  };

}  // namespace react_generic

#endif		// REACT_GENERIC_HPP_
