#include "callback.h"
#include "lua_helper.h"
#include "lua_loader.h"
#include "storm.h"
#include "open_lua_engine.h"
#include "libs_runtime.h"
#include <base/lua/state.h>
#include <base/warcraft3/event.h>
#include <base/warcraft3/jass.h>
#include <base/warcraft3/jass/func_value.h>
#include <base/warcraft3/jass/hook.h>
#include <base/util/singleton.h>
#include <base/util/format.h>
#include <base/hook/fp_call.h>
#include <boost/algorithm/string.hpp>

namespace base { namespace warcraft3 { namespace lua_engine { namespace lua_loader {

	class jass_state
	{
	public:
		jass_state()
			: state_(nullptr)
		{
			register_game_reset_event([this](uintptr_t)
			{
				if (state_)
				{
					state_->close();
					state_ = nullptr;
				}
			});
		}

		lua::state* get()
		{
			if (!state_) state_ = initialize();
			return state_;
		}

	private:
		int open_libs(lua::state* ls)
		{
			ls->requiref("_G",            (lua::cfunction)luaopen_base, 1);      ls->pop(1);
			ls->requiref(LUA_LOADLIBNAME, (lua::cfunction)luaopen_package, 1);   ls->pop(1);
			ls->requiref(LUA_COLIBNAME,   (lua::cfunction)luaopen_coroutine, 1); ls->pop(1);
			ls->requiref(LUA_TABLIBNAME,  (lua::cfunction)luaopen_table, 1);     ls->pop(1);
			ls->requiref(LUA_STRLIBNAME,  (lua::cfunction)luaopen_string, 1);    ls->pop(1);
			ls->requiref(LUA_BITLIBNAME,  (lua::cfunction)luaopen_bit32, 1);     ls->pop(1);
			ls->requiref(LUA_MATHLIBNAME, (lua::cfunction)luaopen_math, 1);      ls->pop(1);
			ls->requiref(LUA_DBLIBNAME,   (lua::cfunction)luaopen_debug, 1);     ls->pop(1);
			
			return 1;
		}

		lua::state* initialize()
		{
			lua::state* ls = (lua::state*)luaL_newstate2();
			open_libs(ls);
			clear_searchers_table(ls);
			open_lua_engine(ls);
			runtime::initialize();
			return ls;
		}

	private:
		lua::state* state_;
	};
	typedef util::singleton_nonthreadsafe<jass_state> jass_state_s;

	uintptr_t RealCheat = 0;
	void __cdecl FakeCheat(jass::jstring_t cheat_str)
	{
		const char* cheat = jass::from_string(cheat_str);

		if (!cheat)
		{
			c_call<uint32_t>(RealCheat, cheat_str);
			return ;
		}

		std::string cheat_s = cheat;
		if (cheat_s.compare(0, 4, "run ") == 0)
		{
			cheat_s = cheat_s.substr(4);
			lua::state* ls = jass_state_s::instance().get();
			ls->getglobal("require");
			ls->pushlstring(cheat_s.c_str(), cheat_s.size());
			safe_call(ls, 1, 1, true);
		}
		else if (cheat_s.compare(0, 9, "exec-lua:") == 0)
		{
			cheat_s = cheat_s.substr(9);
			boost::algorithm::trim(cheat_s);
			if (cheat_s.size() >= 2 && cheat_s[0] == '"' && cheat_s[cheat_s.size() - 1] == '"')
			{
				cheat_s = cheat_s.substr(1, cheat_s.size() - 2);
			}
			lua::state* ls = jass_state_s::instance().get();
			ls->getglobal("require");
			ls->pushlstring(cheat_s.c_str(), cheat_s.size());
			safe_call(ls, 1, 1, true);
		}

		c_call<uint32_t>(RealCheat, cheat_str);
	}


	jass::jstring_t __cdecl EXExecuteScript(jass::jstring_t script)
	{
		lua::state* ls = jass_state_s::instance().get();

		std::string str_script = format("return (%s)", jass::from_trigstring(jass::from_string(script)));
		if (luaL_loadbuffer(ls->self(), str_script.c_str(), str_script.size(), str_script.c_str()) != LUA_OK)
		{
			printf("%s\n", ls->tostring(-1));
			ls->pop(1);
			return 0;
		}

		if (LUA_OK != safe_call(ls, 0, 1, true))
		{
			return 0;
		}

		jass::jstring_t result = 0;
		if (ls->isstring(-1))
		{
			result = jass::create_string(ls->tostring(-1));
		}
		ls->pop(1);
		return result;
	}

	void initialize()
	{
		jass::async_hook("Cheat", (uintptr_t*)&RealCheat, (uintptr_t)FakeCheat);
		jass::japi_add((uintptr_t)EXExecuteScript, "EXExecuteScript", "(S)S");
	}
}}}}
