project(CORE_MODEL_PIPELINE)

include (../ExamplesMacros.cmake)

add_executable(sparta_core_example
  src/main.cpp
  src/Core.cpp
  src/ExampleSimulation.cpp
  src/Fetch.cpp
  src/Decode.cpp
  src/Rename.cpp
  src/Dispatch.cpp
  src/Execute.cpp
  src/ExampleInst.cpp
  src/ROB.cpp
  src/LSU.cpp
  src/BIU.cpp
  src/MSS.cpp
  src/Preloader.cpp
  src/CPU.cpp
  src/CPUFactory.cpp
  src/CPUTopology.cpp
  )

sparta_application(sparta_core_example)

# Add the custom regress/regress_valgrind targets.
add_custom_target (core_example_regress)
add_custom_target (core_example_regress_valgrind)

# NOTE:
# running ctest with --test-action test creates Testing/<datetime>/Test.xml
# that can be loaded into the CI test result tracker
add_custom_command(TARGET core_example_regress          POST_BUILD COMMAND ctest -LE ${VALGRIND_TEST_LABEL} -j10 --test-action test)
add_custom_command(TARGET core_example_regress_valgrind POST_BUILD COMMAND ctest -L  ${VALGRIND_TEST_LABEL} -j10 --test-action test)

#
# Testing
#
include (../../test/TestingMacros.cmake)
sparta_copy(sparta_core_example *.yaml)
sparta_copy(sparta_core_example cpu_layout.alf)
sparta_recursive_copy(sparta_core_example subdir_yamls*)
sparta_recursive_copy(sparta_core_example test_configs)
# build this guy as part of the regress
sparta_regress(sparta_core_example)
sparta_named_test(sparta_core_example_20000cycs sparta_core_example -r 200000)
sparta_named_test(sparta_core_example_fetch_max_ipc sparta_core_example  -i 1M -p top.cpu.core0.fetch.params.fetch_max_ipc true)
sparta_named_test(sparta_core_example_pipeout sparta_core_example -i 1000 -z pipeout -K cpu_layout.alf)
sparta_named_test(sparta_core_example_pipeout_icount sparta_core_example -i 1000 --debug-on-icount 100 -z pipeout -K cpu_layout.alf)
sparta_named_test(sparta_core_example_pipeout_cycle sparta_core_example -i 1000 --debug-on 100 -z pipeout -K cpu_layout.alf)
# Using a standard  YAML causes this test to fail.  Need to investigate Issue #4
# sparta_named_test(sparta_core_example_preload sparta_core_example -i 1000 -p top.cpu.core0.preloader.params.preload_file sample_preload.yaml)
sparta_named_test(sparta_core_example_multicore_yaml_opts_smoke_test sparta_core_example -i 10k --report top multicore_report_opts_basic.yaml out.txt --num-cores 2)
sparta_named_test(sparta_core_example_multireports_yaml_opts_smoke_test sparta_core_example -i 10k --report multireport_independent_triggers.yaml --num-cores 2)
sparta_named_test(sparta_core_example_multireports_yaml_logical_expressions sparta_core_example -i 10k --report multireport_logical_triggers.yaml --num-cores 2)
sparta_named_test(sparta_core_example_multireports_yaml_time_updates sparta_core_example -i 10k --report multireport_time_based_updates.yaml --num-cores 2)
sparta_named_test(sparta_core_example_multireports_yaml_counter_updates sparta_core_example -i 10k --report multireport_counter_based_updates.yaml --num-cores 2)
sparta_named_test(sparta_core_example_multireports_yaml_cycle_updates sparta_core_example -i 10k --report multireport_cycle_based_updates.yaml --num-cores 2)
sparta_named_test(sparta_core_example_multireports_yaml_global_pattern sparta_core_example -i 10k --report multireport_global_pattern.yaml --num-cores 2)
sparta_named_test(sparta_core_example_parameterized_triggers sparta_core_example -i 10k --num-cores 2 --auto-summary off --report parameterized_trigger_points.yaml --parameter top.cpu.core0.params.foo 10)
sparta_named_test(sparta_core_example_list_syntax sparta_core_example -i 10k --report multireport_as_yaml_list.yaml --num-cores 2)
sparta_named_test(sparta_core_example_start_trigger_never_fires sparta_core_example -i 10k --report multireport_start_trigger_never_fires.yaml --num-cores 2)
sparta_named_test(sparta_core_example_tree_node_extensions sparta_core_example -i 10k --extension-file tree_node_extensions.yaml)
sparta_named_test(sparta_core_example_config_file_extensions sparta_core_example -i 10k --config-file config_with_tree_node_extensions.yaml)
sparta_named_test(sparta_core_example_extensions_merged_from_files sparta_core_example -i 10k --config-file config_with_tree_node_extensions.yaml --extension-file tree_node_extensions.yaml)
sparta_named_test(sparta_core_example_arch_file_extensions sparta_core_example -i 10k --arch tree_node_extensions.yaml --arch-search-dir .)
sparta_named_test(sparta_core_example_report_search_paths sparta_core_example -i 10k --report def_file_exists_in_another_dir.yaml --report-search-dir subdir_yamls_1 --report-search-dir subdir_yamls_2/another_subdir)
sparta_named_test(sparta_core_example_named_notifications_when_triggered sparta_core_example -i 10k --report named_notifications_when_triggered.yaml)
sparta_named_test(sparta_core_example_simultaneous_report_yamls sparta_core_example -i 10k --report multireport_r1.yaml --report multireport_r2.yaml)
sparta_named_test(sparta_core_example_simulation_control sparta_core_example -i 10k --control ctrl.yaml)
sparta_named_test(sparta_core_example_simulation_control_separate_files sparta_core_example -i 10k --control ctrl_builtin.yaml --control ctrl_custom.yaml)
sparta_named_test(sparta_core_example_notif_start_trigger_with_update_count sparta_core_example -i 10k --report notif_start_trigger_with_update_count.yaml)
sparta_named_test(sparta_core_example_toggle_triggers sparta_core_example -i 100k --report toggle_expression_triggers.yaml --feature simdb 0)
sparta_named_test(sparta_core_example_parameterized_toggle_triggers sparta_core_example -i 10k --report toggle_parameterized_expression_triggers.yaml --parameter top.cpu.core0.params.foo 1 --feature simdb 0)
sparta_named_test(sparta_core_example_skipped_update_csv_rows sparta_core_example -i 50k --report toggle_trigger_skipping_csv_rows.yaml --feature simdb 0)
sparta_named_test(sparta_core_example_toggle_and_start_triggers_together sparta_core_example -i 50k --report toggle_and_start_triggers_together.yaml --feature simdb 0)
sparta_named_test(sparta_core_example_toggle_trigger_enabled_and_disabled_at_same_time sparta_core_example -i 50k --report toggle_trigger_enabled_disabled_same_time.yaml --feature simdb 0)
sparta_named_test(sparta_core_example_bogus_nans_in_timeseries_reports sparta_core_example -i 50k --report tagged_notif_based_start_trigger.yaml)
sparta_named_test(sparta_core_example_all_descriptor_patterns sparta_core_example -i 50k --report all_descriptor_patterns.yaml --num-cores 2)
sparta_named_test(sparta_core_example_retired_inst_path sparta_core_example -i 10k --report all_descriptor_patterns.yaml --retired-inst-counter-path rename.stats.rename_uop_queue_utilization_count0 --num-cores 2)
sparta_named_test(sparta_core_example_legacy_warmup_notification sparta_core_example -i 50k --report notif_all_threads_warmed_up.yaml --report report_start_on_warmup_notif.yaml --num-cores 2)
sparta_named_test(sparta_core_example_two_reports_json_full_format sparta_core_example -i 10k --report multireports_json_full.yaml)
sparta_named_test(sparta_core_example_two_reports_json_detail_format sparta_core_example -i 10k --report multireports_json_detail.yaml)
sparta_named_test(sparta_core_example_two_reports_json_reduced_format sparta_core_example -i 10k --report multireports_json_reduced.yaml)
sparta_named_test(sparta_core_example_tree_node_extensions_written_to_final_config sparta_core_example -i 10k --arch-search-dir . --arch extensions_in_arch_file.yaml --config-file extensions_in_config_file.yaml --extension-file tree_node_extensions.yaml --write-final-config final_cfg_with_extensions.yaml)
sparta_named_test(sparta_core_example_update_reports_on_demand sparta_core_example -i 10k --report update_report_on_demand.yaml --feature simdb 0)
sparta_named_test(sparta_core_example_context_counter_trigger_expressions sparta_core_example -i 10k --report context_counter_report_triggers.yaml -p top.cpu.core0.params.foo 8k)
sparta_named_test(sparta_core_example_weighted_context_counter_trigger_expressions sparta_core_example -i 10k --report weighted_context_counter_report_triggers.yaml -p top.cpu.core0.params.foo 8k --config-file context_weights.yaml)
if ($ENV{MEMORY_PROFILING_ENABLED})
    sparta_named_test_no_valgrind(sparta_core_example_memory_profiling_to_file_default_settings sparta_core_example -i 10k --log-memory-usage)
    sparta_named_test_no_valgrind(sparta_core_example_memory_profiling_to_file_custom_settings sparta_core_example -i 10k --log-memory-usage memory.yaml)
    sparta_named_test(sparta_core_example_memory_profiling_to_console_default_settings sparta_core_example -i 10k --log-memory-usage 1)
    sparta_named_test(sparta_core_example_memory_profiling_to_console_custom_settings sparta_core_example -i 10k --log-memory-usage memory_to_stdout.yaml)
endif()
sparta_named_test(sparta_core_example_statistic_def_trigger sparta_core_example -i 10k --report statistic_def_triggers.yaml --parameter top.cpu.core0.params.foo 16.5)
sparta_named_test(sparta_core_example_state_tracking sparta_core_example -i 100k --enable-state-tracking states.txt)
sparta_named_test(sparta_core_example_cumulative_stats sparta_core_example -i 10k --report cumulative_stats.yaml)
sparta_named_test(sparta_core_example_statistics_mapping sparta_core_example -i 10k --report statistics_mapping.yaml)
sparta_named_test(sparta_core_example_generate_stats_mapping sparta_core_example -i 10k --report multireport_independent_triggers.yaml --generate-stats-mapping --num-cores 2)
sparta_named_test(sparta_core_example_context_internal_counters_as_triggers sparta_core_example -i 50k --report context_internal_counter_as_trigger.yaml --num-cores 2)
sparta_named_test(sparta_core_example_expanded_context_counter_stats sparta_core_example -i 10k --report context_counters.yaml --num-cores 2)
sparta_named_test(sparta_core_example_unnamed_context_counter_stat sparta_core_example -i 10k --report context_counters.yaml --num-cores 2)
sparta_named_test(sparta_core_example_explicit_context_counter_internals sparta_core_example -i 10k --report context_counters2.yaml --num-cores 2)
sparta_named_test(sparta_core_example_user_defined_calc_as_native_stats sparta_core_example -i 10k --report user_defined_context_counter_stats.yaml)
sparta_named_test(sparta_core_example_arch_with_wildcard sparta_core_example -i 10k --arch-search-dir . --write-final-config final.yaml --arch arch_with_wildcard.yaml)
sparta_named_test(sparta_core_example_config_search_dir sparta_core_example -i 10k --config-file baz_config.yaml --config-search-dir test_configs)
sparta_named_test(sparta_core_example_node_config_search_dir sparta_core_example -i 10k --node-config-file top core_config.yaml --config-search-dir test_configs)
sparta_named_test(sparta_core_example_final_config_search_dir sparta_core_example -i 10k --read-final-config baz_final.yaml --config-search-dir test_configs)
sparta_named_test(sparta_core_example_compact_json_formatting sparta_core_example -i 10k --report all_json_formats.yaml --no-json-pretty-print)
sparta_named_test(sparta_core_example_default_param_config_override sparta_core_example -i 10k -p top.cpu.core0.params.foo 7.89 --config-file parameter_default_config.yaml --write-final-config final.yaml)
sparta_named_test(sparta_core_example_report_yaml_replacements sparta_core_example -i 10k --report placeholders.yaml --report-yaml-replacements TRACENAME my_stats_report CORE0_WARMUP 1200)
sparta_named_test(sparta_core_example_report_specific_yaml_replacements sparta_core_example -i 10k --report foo_descriptor.yaml foo.yaml --report bar_descriptor.yaml bar.yaml)
sparta_named_test(sparta_core_example_omit_stats_with_value_zero sparta_core_example -i 10k --report all_json_formats.yaml --omit-zero-value-stats-from-json_reduced)
sparta_named_test(sparta_core_example_context_counter_update_triggers1 sparta_core_example -i 495 --report context_counter_update_triggers1.yaml --config-file context_weights.yaml -p top.cpu.core0.params.foo 680)
sparta_named_test(sparta_core_example_context_counter_update_triggers2 sparta_core_example -i 550 --report context_counter_update_triggers2.yaml --config-file context_weights.yaml -p top.cpu.core0.params.foo 680)
sparta_named_test(sparta_core_example_context_counter_update_triggers3 sparta_core_example -i 860 --report context_counter_update_triggers3.yaml --config-file context_weights.yaml -p top.cpu.core0.params.foo 680)
sparta_named_test(sparta_core_example_context_counter_update_triggers4 sparta_core_example -i 400 --report context_counter_update_triggers4.yaml --config-file context_weights.yaml -p top.cpu.core0.params.foo 680)
sparta_named_test(sparta_core_example_context_counter_update_triggers5 sparta_core_example -i 415 --report context_counter_update_triggers5.yaml --config-file context_weights.yaml -p top.cpu.core0.params.foo 680)
sparta_named_test(sparta_core_example_context_counter_update_triggers6 sparta_core_example -i 710 --report context_counter_update_triggers6.yaml --config-file context_weights.yaml -p top.cpu.core0.params.foo 680)
sparta_named_test(sparta_core_example_node_config_using_inner_include_yaml sparta_core_example -i 10k --config-file config_with_inner_include.yaml)
sparta_named_test(sparta_core_example_arch_with_extensions sparta_core_example -i 10k --arch arch_with_extensions.yaml --arch-search-dir . --write-final-config final.yaml)
sparta_named_test(sparta_core_example_indiv_tree_node_extensions sparta_core_example -i 10k -p top.cpu.core0.lsu.extension.dog.language_ woof --extension-file tree_node_extensions.yaml -p top.cpu.core0.lsu.extension.cat.language_ meow --write-final-config final.yaml)
sparta_named_test(sparta_core_example_sqldb_timeseries sparta_core_example -i 100k --report all_update_triggers.yaml --feature simdb 1 simdb_si_min_blob_size.yaml)
sparta_named_test(sparta_core_example_sqldb_timeseries_opts1 sparta_core_example -i 100k --report all_update_triggers.yaml --feature simdb 1 simdb_si_opts1.yaml)
sparta_named_test(sparta_core_example_sqldb_timeseries_opts2 sparta_core_example -i 100k --report all_update_triggers.yaml --feature simdb 1 simdb_si_opts2.yaml)
sparta_named_test(sparta_core_example_sqldb_timeseries_opts3 sparta_core_example -i 100k --report all_update_triggers.yaml --feature simdb 1 simdb_si_opts3.yaml)
sparta_named_test(sparta_core_example_sqldb_timeseries_opts4 sparta_core_example -i 100k --report all_update_triggers.yaml --feature simdb 1 simdb_si_opts4.yaml)
sparta_named_test(sparta_core_example_simdb_non_timeseries sparta_core_example -i 10k --report multireport_r1.yaml --feature simdb 1)
#sparta_named_test(sparta_core_example_all_report_formats_no_verif sparta_core_example -i 20k --report all_report_formats.yaml --feature simdb 1)
sparta_named_test(sparta_core_example_expected_dest_file_location sparta_core_example -i 20k --report all_report_formats.yaml --report-verif-output-dir @ --feature simdb 1 --feature simdb-verify 1)
sparta_named_test(sparta_core_example_csv_headers_in_db sparta_core_example -i 100k --report csv_headers_in_db.yaml --feature simdb 1 --feature simdb-verify 1)
#sparta_named_test(sparta_core_example_custom_histogram_stats sparta_core_example -i 100k --report multireport_r3.yaml)
sparta_named_test(sparta_core_example_simdb_configure_opts1 sparta_core_example -i 10k --report all_update_triggers.yaml --simdb-dir .)
sparta_named_test(sparta_core_example_simdb_configure_opts2 sparta_core_example -i 10k --report all_update_triggers.yaml --simdb-dir myfolder)
sparta_named_test(sparta_core_example_simdb_configure_opts3 sparta_core_example -i 10k --report all_update_triggers.yaml --simdb-dir my/sub/folder)
sparta_named_test(sparta_core_example_simdb_configure_opts4 sparta_core_example -i 10k --report all_update_triggers.yaml --simdb-dir myfolder --simdb-enabled-components dbaccess_opts1.yaml)
sparta_named_test_no_valgrind(sparta_core_example_simdb_configure_opts5_wildcard1 sparta_core_example -i 10k --simdb-enabled-components dbaccess_opts2.yaml --feature wildcard-components 1)
sparta_named_test_no_valgrind(sparta_core_example_simdb_configure_opts6_wildcard2 sparta_core_example -i 10k --simdb-enabled-components dbaccess_opts3.yaml)
sparta_named_test_no_valgrind(sparta_core_example_simdb_perf_async_task_controller sparta_core_example -i 50k --feature simdb-perf-async-ctrl 1)
sparta_named_test(sparta_core_example_collect_legacy_reports sparta_core_example -i 10k --report all_json_formats.yaml --collect-legacy-reports collection/dir)
sparta_named_test(sparta_core_example_collect_legacy_reports_specific_format sparta_core_example -i 10k --report all_json_formats.yaml --collect-legacy-reports collection/dir json json_reduced)
