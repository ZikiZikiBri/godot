/*************************************************************************/
/*  tween.cpp                                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "tween.h"

#include "scene/animation/easing_equations.h"
#include "scene/main/node.h"
#include "scene/resources/animation.h"

Tween::interpolater Tween::interpolaters[Tween::TRANS_MAX][Tween::EASE_MAX] = {
	{ &linear::in, &linear::in, &linear::in, &linear::in }, // Linear is the same for each easing.
	{ &sine::in, &sine::out, &sine::in_out, &sine::out_in },
	{ &quint::in, &quint::out, &quint::in_out, &quint::out_in },
	{ &quart::in, &quart::out, &quart::in_out, &quart::out_in },
	{ &quad::in, &quad::out, &quad::in_out, &quad::out_in },
	{ &expo::in, &expo::out, &expo::in_out, &expo::out_in },
	{ &elastic::in, &elastic::out, &elastic::in_out, &elastic::out_in },
	{ &cubic::in, &cubic::out, &cubic::in_out, &cubic::out_in },
	{ &circ::in, &circ::out, &circ::in_out, &circ::out_in },
	{ &bounce::in, &bounce::out, &bounce::in_out, &bounce::out_in },
	{ &back::in, &back::out, &back::in_out, &back::out_in },
};

void Tweener::set_tween(Ref<Tween> p_tween) {
	tween = p_tween;
}

void Tweener::clear_tween() {
	tween.unref();
}

void Tweener::_bind_methods() {
	ADD_SIGNAL(MethodInfo("finished"));
}

void Tween::start_tweeners() {
	if (tweeners.is_empty()) {
		dead = true;
		ERR_FAIL_MSG("Tween without commands, aborting.");
	}

	for (Ref<Tweener> &tweener : tweeners.write[current_step]) {
		tweener->start();
	}
}

Ref<PropertyTweener> Tween::tween_property(Object *p_target, NodePath p_property, Variant p_to, float p_duration) {
	ERR_FAIL_NULL_V(p_target, nullptr);
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "Tween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a Tween that has started. Use stop() first.");

	Variant::Type property_type = p_target->get_indexed(p_property.get_as_property_path().get_subnames()).get_type();
	if (property_type != p_to.get_type()) {
		// Cast p_to between floats and ints to avoid minor annoyances.
		if (property_type == Variant::FLOAT && p_to.get_type() == Variant::INT) {
			p_to = float(p_to);
		} else if (property_type == Variant::INT && p_to.get_type() == Variant::FLOAT) {
			p_to = int(p_to);
		} else {
			ERR_FAIL_V_MSG(Ref<PropertyTweener>(), "Type mismatch between property and final value: " + Variant::get_type_name(property_type) + " and " + Variant::get_type_name(p_to.get_type()));
		}
	}

	Ref<PropertyTweener> tweener = memnew(PropertyTweener(p_target, p_property, p_to, p_duration));
	append(tweener);
	return tweener;
}

Ref<IntervalTweener> Tween::tween_interval(float p_time) {
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "Tween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a Tween that has started. Use stop() first.");

	Ref<IntervalTweener> tweener = memnew(IntervalTweener(p_time));
	append(tweener);
	return tweener;
}

Ref<CallbackTweener> Tween::tween_callback(Callable p_callback) {
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "Tween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a Tween that has started. Use stop() first.");

	Ref<CallbackTweener> tweener = memnew(CallbackTweener(p_callback));
	append(tweener);
	return tweener;
}

Ref<MethodTweener> Tween::tween_method(Callable p_callback, Variant p_from, Variant p_to, float p_duration) {
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "Tween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_V_MSG(started, nullptr, "Can't append to a Tween that has started. Use stop() first.");

	Ref<MethodTweener> tweener = memnew(MethodTweener(p_callback, p_from, p_to, p_duration));
	append(tweener);
	return tweener;
}

void Tween::append(Ref<Tweener> p_tweener) {
	p_tweener->set_tween(this);

	if (parallel_enabled) {
		current_step = MAX(current_step, 0);
	} else {
		current_step++;
	}
	parallel_enabled = default_parallel;

	tweeners.resize(current_step + 1);
	tweeners.write[current_step].push_back(p_tweener);
}

void Tween::stop() {
	started = false;
	running = false;
	dead = false;
	total_time = 0;
}

void Tween::pause() {
	running = false;
}

void Tween::play() {
	ERR_FAIL_COND_MSG(!valid, "Tween invalid. Either finished or created outside scene tree.");
	ERR_FAIL_COND_MSG(dead, "Can't play finished Tween, use stop() first to reset its state.");
	running = true;
}

void Tween::kill() {
	running = false; // For the sake of is_running().
	dead = true;
}

bool Tween::is_running() {
	return running;
}

bool Tween::is_valid() {
	return valid;
}

void Tween::clear() {
	valid = false;

	for (List<Ref<Tweener>> &step : tweeners) {
		for (Ref<Tweener> &tweener : step) {
			tweener->clear_tween();
		}
	}
	tweeners.clear();
}

Ref<Tween> Tween::bind_node(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, this);

	bound_node = p_node->get_instance_id();
	is_bound = true;
	return this;
}

Ref<Tween> Tween::set_process_mode(TweenProcessMode p_mode) {
	process_mode = p_mode;
	return this;
}

Tween::TweenProcessMode Tween::get_process_mode() {
	return process_mode;
}

Ref<Tween> Tween::set_pause_mode(TweenPauseMode p_mode) {
	pause_mode = p_mode;
	return this;
}

Tween::TweenPauseMode Tween::get_pause_mode() {
	return pause_mode;
}

Ref<Tween> Tween::set_parallel(bool p_parallel) {
	default_parallel = p_parallel;
	parallel_enabled = p_parallel;
	return this;
}

Ref<Tween> Tween::set_loops(int p_loops) {
	loops = p_loops;
	return this;
}

Ref<Tween> Tween::set_speed_scale(float p_speed) {
	speed_scale = p_speed;
	return this;
}

Ref<Tween> Tween::set_trans(TransitionType p_trans) {
	default_transition = p_trans;
	return this;
}

Tween::TransitionType Tween::get_trans() {
	return default_transition;
}

Ref<Tween> Tween::set_ease(EaseType p_ease) {
	default_ease = p_ease;
	return this;
}

Tween::EaseType Tween::get_ease() {
	return default_ease;
}

Ref<Tween> Tween::parallel() {
	parallel_enabled = true;
	return this;
}

Ref<Tween> Tween::chain() {
	parallel_enabled = false;
	return this;
}

bool Tween::custom_step(float p_delta) {
	bool r = running;
	running = true;
	bool ret = step(p_delta);
	running = running && r; // Running might turn false when Tween finished.
	return ret;
}

bool Tween::step(float p_delta) {
	if (dead) {
		return false;
	}

	if (!running) {
		return true;
	}

	if (is_bound) {
		Node *bound_node = get_bound_node();
		if (bound_node) {
			if (!bound_node->is_inside_tree()) {
				return true;
			}
		} else {
			return false;
		}
	}

	if (!started) {
		ERR_FAIL_COND_V_MSG(tweeners.is_empty(), false, "Tween started, but has no Tweeners.");
		current_step = 0;
		loops_done = 0;
		total_time = 0;
		start_tweeners();
		started = true;
	}

	float rem_delta = p_delta * speed_scale;
	bool step_active = false;
	total_time += rem_delta;

#ifdef DEBUG_ENABLED
	float initial_delta = rem_delta;
	bool potential_infinite = false;
#endif

	while (rem_delta > 0 && running) {
		float step_delta = rem_delta;
		step_active = false;

		for (Ref<Tweener> &tweener : tweeners.write[current_step]) {
			// Modified inside Tweener.step().
			float temp_delta = rem_delta;
			// Turns to true if any Tweener returns true (i.e. is still not finished).
			step_active = tweener->step(temp_delta) || step_active;
			step_delta = MIN(temp_delta, step_delta);
		}

		rem_delta = step_delta;

		if (!step_active) {
			emit_signal(SNAME("step_finished"), current_step);
			current_step++;

			if (current_step == tweeners.size()) {
				loops_done++;
				if (loops_done == loops) {
					running = false;
					dead = true;
					emit_signal(SNAME("finished"));
				} else {
					emit_signal(SNAME("loop_finished"), loops_done);
					current_step = 0;
					start_tweeners();
#ifdef DEBUG_ENABLED
					if (loops <= 0 && Math::is_equal_approx(rem_delta, initial_delta)) {
						if (!potential_infinite) {
							potential_infinite = true;
						} else {
							// Looped twice without using any time, this is 100% certain infinite loop.
							ERR_FAIL_V_MSG(false, "Infinite loop detected. Check set_loops() description for more info.");
						}
					}
#endif
				}
			} else {
				start_tweeners();
			}
		}
	}

	return true;
}

bool Tween::can_process(bool p_tree_paused) const {
	if (is_bound && pause_mode == TWEEN_PAUSE_BOUND) {
		Node *bound_node = get_bound_node();
		if (bound_node) {
			return bound_node->is_inside_tree() && bound_node->can_process();
		}
	}

	return !p_tree_paused || pause_mode == TWEEN_PAUSE_PROCESS;
}

Node *Tween::get_bound_node() const {
	if (is_bound) {
		return Object::cast_to<Node>(ObjectDB::get_instance(bound_node));
	} else {
		return nullptr;
	}
}

float Tween::get_total_time() const {
	return total_time;
}

real_t Tween::run_equation(TransitionType p_trans_type, EaseType p_ease_type, real_t p_time, real_t p_initial, real_t p_delta, real_t p_duration) {
	if (p_duration == 0) {
		// Special case to avoid dividing by 0 in equations.
		return p_initial + p_delta;
	}

	interpolater func = interpolaters[p_trans_type][p_ease_type];
	return func(p_time, p_initial, p_delta, p_duration);
}

Variant Tween::interpolate_variant(Variant p_initial_val, Variant p_delta_val, float p_time, float p_duration, TransitionType p_trans, EaseType p_ease) {
	ERR_FAIL_INDEX_V(p_trans, TransitionType::TRANS_MAX, Variant());
	ERR_FAIL_INDEX_V(p_ease, EaseType::EASE_MAX, Variant());

	// Special case for bool.
	if (p_initial_val.get_type() == Variant::BOOL) {
		return run_equation(p_trans, p_ease, p_time, p_initial_val, p_delta_val, p_duration) >= 0.5;
	}

	Variant ret = Animation::add_variant(p_initial_val, p_delta_val);
	ret = Animation::interpolate_variant(p_initial_val, ret, run_equation(p_trans, p_ease, p_time, 0.0, 1.0, p_duration));
	return ret;
}

void Tween::_bind_methods() {
	ClassDB::bind_method(D_METHOD("tween_property", "object", "property", "final_val", "duration"), &Tween::tween_property);
	ClassDB::bind_method(D_METHOD("tween_interval", "time"), &Tween::tween_interval);
	ClassDB::bind_method(D_METHOD("tween_callback", "callback"), &Tween::tween_callback);
	ClassDB::bind_method(D_METHOD("tween_method", "method", "from", "to", "duration"), &Tween::tween_method);

	ClassDB::bind_method(D_METHOD("custom_step", "delta"), &Tween::custom_step);
	ClassDB::bind_method(D_METHOD("stop"), &Tween::stop);
	ClassDB::bind_method(D_METHOD("pause"), &Tween::pause);
	ClassDB::bind_method(D_METHOD("play"), &Tween::play);
	ClassDB::bind_method(D_METHOD("kill"), &Tween::kill);
	ClassDB::bind_method(D_METHOD("get_total_elapsed_time"), &Tween::get_total_time);

	ClassDB::bind_method(D_METHOD("is_running"), &Tween::is_running);
	ClassDB::bind_method(D_METHOD("is_valid"), &Tween::is_valid);
	ClassDB::bind_method(D_METHOD("bind_node", "node"), &Tween::bind_node);
	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &Tween::set_process_mode);
	ClassDB::bind_method(D_METHOD("set_pause_mode", "mode"), &Tween::set_pause_mode);

	ClassDB::bind_method(D_METHOD("set_parallel", "parallel"), &Tween::set_parallel, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_loops", "loops"), &Tween::set_loops, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_speed_scale", "speed"), &Tween::set_speed_scale);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &Tween::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &Tween::set_ease);

	ClassDB::bind_method(D_METHOD("parallel"), &Tween::parallel);
	ClassDB::bind_method(D_METHOD("chain"), &Tween::chain);

	ClassDB::bind_static_method("Tween", D_METHOD("interpolate_value", "initial_value", "delta_value", "elapsed_time", "duration", "trans_type", "ease_type"), &Tween::interpolate_variant);

	ADD_SIGNAL(MethodInfo("step_finished", PropertyInfo(Variant::INT, "idx")));
	ADD_SIGNAL(MethodInfo("loop_finished", PropertyInfo(Variant::INT, "loop_count")));
	ADD_SIGNAL(MethodInfo("finished"));

	BIND_ENUM_CONSTANT(TWEEN_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(TWEEN_PROCESS_IDLE);

	BIND_ENUM_CONSTANT(TWEEN_PAUSE_BOUND);
	BIND_ENUM_CONSTANT(TWEEN_PAUSE_STOP);
	BIND_ENUM_CONSTANT(TWEEN_PAUSE_PROCESS);

	BIND_ENUM_CONSTANT(TRANS_LINEAR);
	BIND_ENUM_CONSTANT(TRANS_SINE);
	BIND_ENUM_CONSTANT(TRANS_QUINT);
	BIND_ENUM_CONSTANT(TRANS_QUART);
	BIND_ENUM_CONSTANT(TRANS_QUAD);
	BIND_ENUM_CONSTANT(TRANS_EXPO);
	BIND_ENUM_CONSTANT(TRANS_ELASTIC);
	BIND_ENUM_CONSTANT(TRANS_CUBIC);
	BIND_ENUM_CONSTANT(TRANS_CIRC);
	BIND_ENUM_CONSTANT(TRANS_BOUNCE);
	BIND_ENUM_CONSTANT(TRANS_BACK);

	BIND_ENUM_CONSTANT(EASE_IN);
	BIND_ENUM_CONSTANT(EASE_OUT);
	BIND_ENUM_CONSTANT(EASE_IN_OUT);
	BIND_ENUM_CONSTANT(EASE_OUT_IN);
}

Tween::Tween() {
	ERR_FAIL_MSG("Tween can't be created directly. Use create_tween() method.");
}

Tween::Tween(bool p_valid) {
	valid = p_valid;
}

Ref<PropertyTweener> PropertyTweener::from(Variant p_value) {
	initial_val = p_value;
	do_continue = false;
	return this;
}

Ref<PropertyTweener> PropertyTweener::from_current() {
	do_continue = false;
	return this;
}

Ref<PropertyTweener> PropertyTweener::as_relative() {
	relative = true;
	return this;
}

Ref<PropertyTweener> PropertyTweener::set_trans(Tween::TransitionType p_trans) {
	trans_type = p_trans;
	return this;
}

Ref<PropertyTweener> PropertyTweener::set_ease(Tween::EaseType p_ease) {
	ease_type = p_ease;
	return this;
}

Ref<PropertyTweener> PropertyTweener::set_delay(float p_delay) {
	delay = p_delay;
	return this;
}

void PropertyTweener::start() {
	elapsed_time = 0;
	finished = false;

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		WARN_PRINT("Target object freed before starting, aborting Tweener.");
		return;
	}

	if (do_continue) {
		initial_val = target_instance->get_indexed(property);
	}

	if (relative) {
		final_val = Animation::add_variant(initial_val, base_final_val);
	}

	delta_val = Animation::subtract_variant(final_val, initial_val);
}

bool PropertyTweener::step(float &r_delta) {
	if (finished) {
		// This is needed in case there's a parallel Tweener with longer duration.
		return false;
	}

	Object *target_instance = ObjectDB::get_instance(target);
	if (!target_instance) {
		return false;
	}
	elapsed_time += r_delta;

	if (elapsed_time < delay) {
		r_delta = 0;
		return true;
	}

	float time = MIN(elapsed_time - delay, duration);
	if (time < duration) {
		target_instance->set_indexed(property, tween->interpolate_variant(initial_val, delta_val, time, duration, trans_type, ease_type));
		r_delta = 0;
		return true;
	} else {
		target_instance->set_indexed(property, final_val);
		finished = true;
		r_delta = elapsed_time - delay - duration;
		emit_signal(SNAME("finished"));
		return false;
	}
}

void PropertyTweener::set_tween(Ref<Tween> p_tween) {
	tween = p_tween;
	if (trans_type == Tween::TRANS_MAX) {
		trans_type = tween->get_trans();
	}
	if (ease_type == Tween::EASE_MAX) {
		ease_type = tween->get_ease();
	}
}

void PropertyTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("from", "value"), &PropertyTweener::from);
	ClassDB::bind_method(D_METHOD("from_current"), &PropertyTweener::from_current);
	ClassDB::bind_method(D_METHOD("as_relative"), &PropertyTweener::as_relative);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &PropertyTweener::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &PropertyTweener::set_ease);
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &PropertyTweener::set_delay);
}

PropertyTweener::PropertyTweener(Object *p_target, NodePath p_property, Variant p_to, float p_duration) {
	target = p_target->get_instance_id();
	property = p_property.get_as_property_path().get_subnames();
	initial_val = p_target->get_indexed(property);
	base_final_val = p_to;
	final_val = base_final_val;
	duration = p_duration;
}

PropertyTweener::PropertyTweener() {
	ERR_FAIL_MSG("Can't create empty PropertyTweener. Use get_tree().tween_property() or tween_property() instead.");
}

void IntervalTweener::start() {
	elapsed_time = 0;
	finished = false;
}

bool IntervalTweener::step(float &r_delta) {
	if (finished) {
		return false;
	}

	elapsed_time += r_delta;

	if (elapsed_time < duration) {
		r_delta = 0;
		return true;
	} else {
		finished = true;
		r_delta = elapsed_time - duration;
		emit_signal(SNAME("finished"));
		return false;
	}
}

IntervalTweener::IntervalTweener(float p_time) {
	duration = p_time;
}

IntervalTweener::IntervalTweener() {
	ERR_FAIL_MSG("Can't create empty IntervalTweener. Use get_tree().tween_interval() instead.");
}

Ref<CallbackTweener> CallbackTweener::set_delay(float p_delay) {
	delay = p_delay;
	return this;
}

void CallbackTweener::start() {
	elapsed_time = 0;
	finished = false;
}

bool CallbackTweener::step(float &r_delta) {
	if (finished) {
		return false;
	}

	elapsed_time += r_delta;
	if (elapsed_time >= delay) {
		Variant result;
		Callable::CallError ce;
		callback.callp(nullptr, 0, result, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(false, "Error calling method from CallbackTweener: " + Variant::get_callable_error_text(callback, nullptr, 0, ce));
		}

		finished = true;
		r_delta = elapsed_time - delay;
		emit_signal(SNAME("finished"));
		return false;
	}

	r_delta = 0;
	return true;
}

void CallbackTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &CallbackTweener::set_delay);
}

CallbackTweener::CallbackTweener(Callable p_callback) {
	callback = p_callback;
}

CallbackTweener::CallbackTweener() {
	ERR_FAIL_MSG("Can't create empty CallbackTweener. Use get_tree().tween_callback() instead.");
}

Ref<MethodTweener> MethodTweener::set_delay(float p_delay) {
	delay = p_delay;
	return this;
}

Ref<MethodTweener> MethodTweener::set_trans(Tween::TransitionType p_trans) {
	trans_type = p_trans;
	return this;
}

Ref<MethodTweener> MethodTweener::set_ease(Tween::EaseType p_ease) {
	ease_type = p_ease;
	return this;
}

void MethodTweener::start() {
	elapsed_time = 0;
	finished = false;
}

bool MethodTweener::step(float &r_delta) {
	if (finished) {
		return false;
	}

	elapsed_time += r_delta;

	if (elapsed_time < delay) {
		r_delta = 0;
		return true;
	}

	Variant current_val;
	float time = MIN(elapsed_time - delay, duration);
	if (time < duration) {
		current_val = tween->interpolate_variant(initial_val, delta_val, time, duration, trans_type, ease_type);
	} else {
		current_val = final_val;
	}
	const Variant **argptr = (const Variant **)alloca(sizeof(Variant *));
	argptr[0] = &current_val;

	Variant result;
	Callable::CallError ce;
	callback.callp(argptr, 1, result, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_FAIL_V_MSG(false, "Error calling method from MethodTweener: " + Variant::get_callable_error_text(callback, argptr, 1, ce));
	}

	if (time < duration) {
		r_delta = 0;
		return true;
	} else {
		finished = true;
		r_delta = elapsed_time - delay - duration;
		emit_signal(SNAME("finished"));
		return false;
	}
}

void MethodTweener::set_tween(Ref<Tween> p_tween) {
	tween = p_tween;
	if (trans_type == Tween::TRANS_MAX) {
		trans_type = tween->get_trans();
	}
	if (ease_type == Tween::EASE_MAX) {
		ease_type = tween->get_ease();
	}
}

void MethodTweener::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_delay", "delay"), &MethodTweener::set_delay);
	ClassDB::bind_method(D_METHOD("set_trans", "trans"), &MethodTweener::set_trans);
	ClassDB::bind_method(D_METHOD("set_ease", "ease"), &MethodTweener::set_ease);
}

MethodTweener::MethodTweener(Callable p_callback, Variant p_from, Variant p_to, float p_duration) {
	callback = p_callback;
	initial_val = p_from;
	delta_val = Animation::subtract_variant(p_to, p_from);
	final_val = p_to;
	duration = p_duration;
}

MethodTweener::MethodTweener() {
	ERR_FAIL_MSG("Can't create empty MethodTweener. Use get_tree().tween_method() instead.");
}
