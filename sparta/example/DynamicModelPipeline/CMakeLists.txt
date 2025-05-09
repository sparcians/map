project(DYNAMIC_MODEL_PIPELINE)

include(../ExamplesMacros.cmake)
include(${SPARTA_BASE}/cmake/sparta-config.cmake)

add_executable(dynamic_model_pipeline
    src/main.cpp
    src/Core.cpp
    src/ExampleSimulation.cpp
    ../CoreModel/src/Fetch.cpp
    ../CoreModel/src/Decode.cpp
    ../CoreModel/src/Rename.cpp
    ../CoreModel/src/Dispatch.cpp
    ../CoreModel/src/Execute.cpp
    ../CoreModel/src/ROB.cpp
    ../CoreModel/src/LSU.cpp
    ../CoreModel/src/BIU.cpp
    ../CoreModel/src/MSS.cpp
    ../CoreModel/src/Preloader.cpp
    ../CoreModel/src/ExampleInst.cpp
)

include_directories(../CoreModel/src)
sparta_application(dynamic_model_pipeline)

#
# Testing
#
sparta_copy(dynamic_model_pipeline *.yaml)
sparta_copy(dynamic_model_pipeline cpu_layout.alf)
sparta_recursive_copy(dynamic_model_pipeline subdir_yamls*)
sparta_recursive_copy(dynamic_model_pipeline test_configs)
# build this guy as part of the regress
sparta_regress(dynamic_model_pipeline)
sparta_named_test(dynamic_model_pipeline_20000cycs dynamic_model_pipeline -r 200000)
sparta_named_test(dynamic_model_pipeline_fetch_max_ipc dynamic_model_pipeline  -i 1M -p top.core0.fetch.params.fetch_max_ipc true)
sparta_named_test(dynamic_model_pipeline_pipeout dynamic_model_pipeline -i 1000 -z pipeout -K cpu_layout.alf)
sparta_named_test(dynamic_model_pipeline_pipeout_icount dynamic_model_pipeline -i 1000 --debug-on-icount 100 -z pipeout -K cpu_layout.alf)
sparta_named_test(dynamic_model_pipeline_pipeout_cycle dynamic_model_pipeline -i 1000 --debug-on 100 -z pipeout -K cpu_layout.alf)
sparta_named_test(dynamic_model_pipeline_preload dynamic_model_pipeline -i 1000 -p top.core0.preloader.params.preload_file sample_preload.yaml)
sparta_named_test(dynamic_model_pipeline_multicore_yaml_opts_smoke_test dynamic_model_pipeline -i 10k --report top multicore_report_opts_basic.yaml out.txt --num-cores 2)
sparta_named_test(dynamic_model_pipeline_multireports_yaml_opts_smoke_test dynamic_model_pipeline -i 10k --report multireport_independent_triggers.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_multireports_yaml_logical_expressions dynamic_model_pipeline -i 10k --report multireport_logical_triggers.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_multireports_yaml_time_updates dynamic_model_pipeline -i 10k --report multireport_time_based_updates.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_multireports_yaml_counter_updates dynamic_model_pipeline -i 10k --report multireport_counter_based_updates.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_multireports_yaml_cycle_updates dynamic_model_pipeline -i 10k --report multireport_cycle_based_updates.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_multireports_yaml_global_pattern dynamic_model_pipeline -i 10k --report multireport_global_pattern.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_parameterized_triggers dynamic_model_pipeline -i 10k --num-cores 2 --auto-summary off --report parameterized_trigger_points.yaml --parameter top.core0.params.foo 10)
sparta_named_test(dynamic_model_pipeline_list_syntax dynamic_model_pipeline -i 10k --report multireport_as_yaml_list.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_start_trigger_never_fires dynamic_model_pipeline -i 10k --report multireport_start_trigger_never_fires.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_tree_node_extensions dynamic_model_pipeline -i 10k --extension-file tree_node_extensions.yaml)
sparta_named_test(dynamic_model_pipeline_config_file_extensions dynamic_model_pipeline -i 10k --config-file config_with_tree_node_extensions.yaml)
sparta_named_test(dynamic_model_pipeline_extensions_merged_from_files dynamic_model_pipeline -i 10k --config-file config_with_tree_node_extensions.yaml --extension-file tree_node_extensions.yaml)
sparta_named_test(dynamic_model_pipeline_arch_file_extensions dynamic_model_pipeline -i 10k --arch tree_node_extensions.yaml --arch-search-dir .)
sparta_named_test(dynamic_model_pipeline_report_search_paths dynamic_model_pipeline -i 10k --report def_file_exists_in_another_dir.yaml --report-search-dir subdir_yamls_1 --report-search-dir subdir_yamls_2/another_subdir)
sparta_named_test(dynamic_model_pipeline_named_notifications_when_triggered dynamic_model_pipeline -i 10k --report named_notifications_when_triggered.yaml)
sparta_named_test(dynamic_model_pipeline_simultaneous_report_yamls dynamic_model_pipeline -i 10k --report multireport_r1.yaml --report multireport_r2.yaml)
sparta_named_test(dynamic_model_pipeline_simulation_control dynamic_model_pipeline -i 10k --control ctrl.yaml)
sparta_named_test(dynamic_model_pipeline_simulation_control_separate_files dynamic_model_pipeline -i 10k --control ctrl_builtin.yaml --control ctrl_custom.yaml)
sparta_named_test(dynamic_model_pipeline_notif_start_trigger_with_update_count dynamic_model_pipeline -i 10k --report notif_start_trigger_with_update_count.yaml)
sparta_named_test(dynamic_model_pipeline_toggle_triggers dynamic_model_pipeline -i 100k --report toggle_expression_triggers.yaml)
sparta_named_test(dynamic_model_pipeline_parameterized_toggle_triggers dynamic_model_pipeline -i 10k --report toggle_parameterized_expression_triggers.yaml --parameter top.core0.params.foo 1)
sparta_named_test(dynamic_model_pipeline_skipped_update_csv_rows dynamic_model_pipeline -i 50k --report toggle_trigger_skipping_csv_rows.yaml)
sparta_named_test(dynamic_model_pipeline_toggle_and_start_triggers_together dynamic_model_pipeline -i 50k --report toggle_and_start_triggers_together.yaml)
sparta_named_test(dynamic_model_pipeline_toggle_trigger_enabled_and_disabled_at_same_time dynamic_model_pipeline -i 50k --report toggle_trigger_enabled_disabled_same_time.yaml)
sparta_named_test(dynamic_model_pipeline_bogus_nans_in_timeseries_reports dynamic_model_pipeline -i 50k --report tagged_notif_based_start_trigger.yaml)
sparta_named_test(dynamic_model_pipeline_all_descriptor_patterns dynamic_model_pipeline -i 50k --report all_descriptor_patterns.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_retired_inst_path dynamic_model_pipeline -i 10k --report all_descriptor_patterns.yaml --retired-inst-counter-path rename.stats.rename_uop_queue_utilization_count0 --num-cores 2)
sparta_named_test(dynamic_model_pipeline_legacy_warmup_notification dynamic_model_pipeline -i 50k --report notif_all_threads_warmed_up.yaml --report report_start_on_warmup_notif.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_two_reports_json_full_format dynamic_model_pipeline -i 10k --report multireports_json_full.yaml)
sparta_named_test(dynamic_model_pipeline_two_reports_json_detail_format dynamic_model_pipeline -i 10k --report multireports_json_detail.yaml)
sparta_named_test(dynamic_model_pipeline_two_reports_json_reduced_format dynamic_model_pipeline -i 10k --report multireports_json_reduced.yaml)
sparta_named_test(dynamic_model_pipeline_tree_node_extensions_written_to_final_config dynamic_model_pipeline -i 10k --arch-search-dir . --arch extensions_in_arch_file.yaml --config-file extensions_in_config_file.yaml --extension-file tree_node_extensions.yaml --write-final-config final_cfg_with_extensions.yaml)
sparta_named_test(dynamic_model_pipeline_update_reports_on_demand dynamic_model_pipeline -i 10k --report update_report_on_demand.yaml)
sparta_named_test(dynamic_model_pipeline_context_counter_trigger_expressions dynamic_model_pipeline -i 10k --report context_counter_report_triggers.yaml -p top.core0.params.foo 8k)
sparta_named_test(dynamic_model_pipeline_weighted_context_counter_trigger_expressions dynamic_model_pipeline -i 10k --report weighted_context_counter_report_triggers.yaml -p top.core0.params.foo 8k --config-file context_weights.yaml)
if ($ENV{MEMORY_PROFILING_ENABLED})
    sparta_named_test_no_valgrind(dynamic_model_pipeline_memory_profiling_to_file_default_settings dynamic_model_pipeline -i 10k --log-memory-usage)
    sparta_named_test_no_valgrind(dynamic_model_pipeline_memory_profiling_to_file_custom_settings dynamic_model_pipeline -i 10k --log-memory-usage memory.yaml)
    sparta_named_test(dynamic_model_pipeline_memory_profiling_to_console_default_settings dynamic_model_pipeline -i 10k --log-memory-usage 1)
    sparta_named_test(dynamic_model_pipeline_memory_profiling_to_console_custom_settings dynamic_model_pipeline -i 10k --log-memory-usage memory_to_stdout.yaml)
endif()
sparta_named_test(dynamic_model_pipeline_statistic_def_trigger dynamic_model_pipeline -i 10k --report statistic_def_triggers.yaml --parameter top.core0.params.foo 16.5)
sparta_named_test(dynamic_model_pipeline_state_tracking dynamic_model_pipeline -i 100k --enable-state-tracking states.txt)
sparta_named_test(dynamic_model_pipeline_cumulative_stats dynamic_model_pipeline -i 10k --report cumulative_stats.yaml)
sparta_named_test(dynamic_model_pipeline_statistics_mapping dynamic_model_pipeline -i 10k --report statistics_mapping.yaml)
sparta_named_test(dynamic_model_pipeline_generate_stats_mapping dynamic_model_pipeline -i 10k --report multireport_independent_triggers.yaml --generate-stats-mapping --num-cores 2)
sparta_named_test(dynamic_model_pipeline_context_internal_counters_as_triggers dynamic_model_pipeline -i 50k --report context_internal_counter_as_trigger.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_expanded_context_counter_stats dynamic_model_pipeline -i 10k --report context_counters.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_unnamed_context_counter_stat dynamic_model_pipeline -i 10k --report context_counters.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_explicit_context_counter_internals dynamic_model_pipeline -i 10k --report context_counters2.yaml --num-cores 2)
sparta_named_test(dynamic_model_pipeline_user_defined_calc_as_native_stats dynamic_model_pipeline -i 10k --report user_defined_context_counter_stats.yaml)
sparta_named_test(dynamic_model_pipeline_arch_with_wildcard dynamic_model_pipeline -i 10k --arch-search-dir . --write-final-config final.yaml --arch arch_with_wildcard.yaml)
sparta_named_test(dynamic_model_pipeline_config_search_dir dynamic_model_pipeline -i 10k --config-file baz_config.yaml --config-search-dir test_configs)
sparta_named_test(dynamic_model_pipeline_node_config_search_dir dynamic_model_pipeline -i 10k --node-config-file top core_config.yaml --config-search-dir test_configs)
sparta_named_test(dynamic_model_pipeline_final_config_search_dir dynamic_model_pipeline -i 10k --read-final-config baz_final.yaml --config-search-dir test_configs)
sparta_named_test(dynamic_model_pipeline_compact_json_formatting dynamic_model_pipeline -i 10k --report all_json_formats.yaml --no-json-pretty-print)
sparta_named_test(dynamic_model_pipeline_default_param_config_override dynamic_model_pipeline -i 10k -p top.core0.params.foo 7.89 --config-file parameter_default_config.yaml --write-final-config final.yaml)
sparta_named_test(dynamic_model_pipeline_report_yaml_replacements dynamic_model_pipeline -i 10k --report placeholders.yaml --report-yaml-replacements TRACENAME my_stats_report CORE0_WARMUP 1200)
sparta_named_test(dynamic_model_pipeline_report_specific_yaml_replacements dynamic_model_pipeline -i 10k --report foo_descriptor.yaml foo.yaml --report bar_descriptor.yaml bar.yaml)
sparta_named_test(dynamic_model_pipeline_omit_stats_with_value_zero dynamic_model_pipeline -i 10k --report all_json_formats.yaml --omit-zero-value-stats-from-json_reduced)
sparta_named_test(dynamic_model_pipeline_context_counter_update_triggers1 dynamic_model_pipeline -i 495 --report context_counter_update_triggers1.yaml --config-file context_weights.yaml -p top.core0.params.foo 680)
sparta_named_test(dynamic_model_pipeline_context_counter_update_triggers2 dynamic_model_pipeline -i 550 --report context_counter_update_triggers2.yaml --config-file context_weights.yaml -p top.core0.params.foo 680)
sparta_named_test(dynamic_model_pipeline_context_counter_update_triggers3 dynamic_model_pipeline -i 860 --report context_counter_update_triggers3.yaml --config-file context_weights.yaml -p top.core0.params.foo 680)
sparta_named_test(dynamic_model_pipeline_context_counter_update_triggers4 dynamic_model_pipeline -i 400 --report context_counter_update_triggers4.yaml --config-file context_weights.yaml -p top.core0.params.foo 680)
sparta_named_test(dynamic_model_pipeline_context_counter_update_triggers5 dynamic_model_pipeline -i 415 --report context_counter_update_triggers5.yaml --config-file context_weights.yaml -p top.core0.params.foo 680)
sparta_named_test(dynamic_model_pipeline_context_counter_update_triggers6 dynamic_model_pipeline -i 710 --report context_counter_update_triggers6.yaml --config-file context_weights.yaml -p top.core0.params.foo 680)
sparta_named_test(dynamic_model_pipeline_node_config_using_inner_include_yaml dynamic_model_pipeline -i 10k --config-file config_with_inner_include.yaml)
sparta_named_test(dynamic_model_pipeline_arch_with_extensions dynamic_model_pipeline -i 10k --arch arch_with_extensions.yaml --arch-search-dir . --write-final-config final.yaml)
# Extensions supplied that are not bound are now errors
#sparta_named_test(dynamic_model_pipeline_indiv_tree_node_extensions dynamic_model_pipeline -i 10k -p top.core0.lsu.extension.dog.language_ woof --extension-file tree_node_extensions.yaml -p top.core0.lsu.extension.cat.language_ meow --write-final-config final.yaml)
sparta_named_test(dynamic_model_pipeline_indiv_tree_node_extensions dynamic_model_pipeline -i 10k --extension-file tree_node_extensions.yaml -p top.core0.lsu.extension.cat.language_ meow --write-final-config final.yaml)
