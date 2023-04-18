#ifndef _PARADOX1738_H_
#define _PARADOX1738_H_

/**
 * @brief initialize the lightbulb lowlevel module
 *
 * @param none
 *
 * @return none
 */
void lightbulb_init(void);

/**
 * @brief deinitialize the lightbulb's lowlevel module
 *
 * @param none
 *
 * @return none
 */
void lightbulb_deinit(void);

/**
 * @brief turn on/off the lowlevel lightbulb
 *
 * @param value The "On" value
 *
 * @return none
 */
int lightbulb_set_on(bool value);

/**
 * @brief set the saturation of the lowlevel lightbulb
 *
 * @param value The Saturation value
 *
 * @return 
 *     - 0 : OK
 *     - others : fail
 */
int lightbulb_set_saturation(float value);

/**
 * @brief set the hue of the lowlevel lightbulb
 *
 * @param value The Hue value
 *
 * @return 
 *     - 0 : OK
 *     - others : fail
 */
int lightbulb_set_hue(float value);

/**
 * @brief set the brightness of the lowlevel lightbulb
 *
 * @param value The Brightness value
 *
 * @return 
 *     - 0 : OK
 *     - others : fail
 */
int lightbulb_set_brightness(int value);

#endif /* _PARADOX1738_H_ */
