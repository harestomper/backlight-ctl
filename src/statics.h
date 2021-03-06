/*
 * statics.h
 *
 *  Created on: 9 Feb. 2018 г.
 *      Author: Voldemar Khramtsov <harestomper@gmail.com>
 *
 */

#ifndef SRC_STATICS_H_
#define SRC_STATICS_H_

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
option_t const* __options (void);
#define statics_options (__options ())
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
type_t const* __types (void);
#define statics_types (__types ())
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
default_value_t const* __defaults (void);
#define statics_defaults (__defaults ())
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif /* SRC_STATICS_H_ */
