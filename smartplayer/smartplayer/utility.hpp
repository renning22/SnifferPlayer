#ifndef _UTILITY_HPP_
#define _UTILITY_HPP_

template<class GetFunc,class SetFunc,class Value>
void change_gradually(GetFunc get_func,SetFunc set_func,Value t,Value r,Value k)
{
	Value x = get_func();
	Value scale = fabs(x-t);
	Value sign = (x-t)/fabs(x-t);
	if( scale < r )
	{
		set_func( t );
		return;
	}
	Value change = -sign*k*scale;
	set_func( x+change );
}


template<class Object,class Aborter,class Sleeper>
bool block_wait(Object object,Aborter aborter,Sleeper sleeper)
{
	for(;;)
	{
		if( object() )
			return true;
		if( aborter() )
			return false;
		sleeper();
	}
}

#endif
