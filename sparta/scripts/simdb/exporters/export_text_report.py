import io, os, zlib, struct
from .utils import FormatNumber

class SimulationInfo:
    @classmethod
    def GetHeaderPairs(cls, db_conn):
        cursor = db_conn.cursor()
        cmd = "SELECT HeaderName, HeaderValue FROM SimulationInfoHeaderPairs"
        cursor.execute(cmd)

        header_pairs = []
        for p in cursor.fetchall():
            header_pairs.append((p[0], p[1]))

        return header_pairs

    @classmethod
    def Stringize(cls, db_conn, out, line_start = "# ", line_end = "\n", show_field_names = True):
        pairs = cls.GetHeaderPairs(db_conn)
        for p in pairs:
            out.write(line_start)
            if show_field_names:
                # out << std::left << std::setw(10) << (p.first + ":");
                name = p[0] + ":"
                out.write(f"{name:<10}")

            # out << p.second << line_end;
            out.write(f"{p[1]}{line_end}")

class StatisticInstance:
    def __init__(self, name, location, desc, stat_value_getter):
        self.name = name
        self.location = location
        self.desc = desc
        self.stat_value_getter = stat_value_getter

    def GetName(self):
        return self.name
    
    def GetLocation(self):
        return self.location

    def GetDesc(self, unused=False):
        assert unused == False
        return self.desc

    def GetValue(self):
        return self.stat_value_getter.GetNext()

class StatValueGetter:
    def __init__(self, stats_values):
        self.stats_values = stats_values
        self.index = 0

    def GetNext(self):
        if self.index >= len(self.stats_values):
            raise IndexError("No more values in stats blob")
        value = self.stats_values[self.index]
        self.index += 1
        return value

def GetStatsValuesGetter(cursor, dest_file):
    dest_file_name = os.path.basename(dest_file)
    cmd = f"SELECT Data, IsCompressed FROM CollectionRecords WHERE Notes='{dest_file_name}'"
    cursor.execute(cmd)
    stats_blob, is_compressed = cursor.fetchone()
    if is_compressed:
        stats_blob = zlib.decompress(stats_blob)

    # Turn the stats blob (byte vector) into a vector of doubles.
    assert len(stats_blob) % (2+8) == 0, "Invalid stats blob length"
    stats_values = []
    for i in range(0, len(stats_blob), 2+8):
        # The first value is the collectable ID, the second is the value.
        # We don't need the collectable ID here.
        val = struct.unpack("d", stats_blob[i+2:i+10])[0]
        val = FormatNumber(val, as_string=False)
        stats_values.append(val)

    return StatValueGetter(stats_values)

class TextReportExporter:
    DEFAULT_REPORT_PREFIX = "Report "

    def __init__(self):
        self.stat_value_getter = None
        self.show_sim_info = True
        self.show_descs = False
        self.write_contentless_reports = True
        self.indent_subreports = True
        self.report_prefix = self.DEFAULT_REPORT_PREFIX
        self.quote_report_names = True
        self.show_report_range = True
        self.val_col = 0

    def Export(self, dest_file, descriptor_id, db_conn):
        out = io.StringIO()
        if self.GetShowSimInfo():
            # out << sparta::SimulationInfo::getInstance().stringize("", "\n") << std::endl << std::endl;
            SimulationInfo.Stringize(db_conn, out, line_start="", line_end="\n")
            out.write("\n\n")

        self.stat_value_getter = GetStatsValuesGetter(db_conn.cursor(), dest_file)

        # Get the root report ID
        cmd = f"SELECT Id FROM Reports WHERE ReportDescID = {descriptor_id} AND ParentReportID = 0"
        cursor = db_conn.cursor()
        cursor.execute(cmd)
        row = cursor.fetchone()
        if row is None:
            raise ValueError(f"Report descriptor {descriptor_id} does not have a root report")

        root_report_id = row[0]
        self.__Dump(db_conn, out, descriptor_id, root_report_id, 0)

        with open(dest_file, "w") as fout:
            fout.write(out.getvalue())

    def GetShowSimInfo(self):
        return self.show_sim_info

    def GetShowDescriptions(self):
        return self.show_descs

    def GetWriteContentlessReports(self):
        return self.write_contentless_reports

    def GetIndentSubreports(self):
        return self.indent_subreports

    def GetReportPrefix(self):
        return self.report_prefix

    def GetQuoteReportNames(self):
        return self.quote_report_names

    def GetShowReportRange(self):
        return self.show_report_range

    def GetValueColumn(self):
        return self.val_col

    def __Dump(self, db_conn, out, descriptor_id, report_id, depth):
        cursor = db_conn.cursor()
        cmd = f"SELECT Name, StartTick, EndTick FROM Reports WHERE Id = {report_id}"
        cursor.execute(cmd)
        row = cursor.fetchone()

        if not row:
            return

        report_name, report_start, report_end = row

        if report_end == -1:
            cmd = "SELECT SimEndTick FROM SimulationInfo WHERE Id = 1"
            cursor.execute(cmd)
            report_end = cursor.fetchone()[0]

        #const bool show_descs =
        #    (r->getStyle("show_descriptions", getShowDescriptions() ? "true" : "false") == "true");
        cmd = f"SELECT StyleValue FROM ReportStyles WHERE ReportDescID = {descriptor_id} AND ReportID = {report_id} AND StyleName = 'show_descriptions'"
        cursor.execute(cmd)

        show_descs = cursor.fetchone()
        if show_descs is None:
            show_descs = self.GetShowDescriptions()
        else:
            assert show_descs[0].lower() in ("true", "false")
            show_descs = show_descs[0].lower() == "true"

        # const std::string INDENT_STR = "  ";
        indent_str = "  "

        # const std::string ADDITIONAL_STAT_INDENT = "  ";
        additional_stat_indent = "  "

        # if(write_contentless_reports_ || hasStatistics_(r)){
        if self.GetWriteContentlessReports() or self.__HasStatistics(db_conn, report_id):
            # std::stringstream indent;
            # if(indent_subreports_){
            #     for(uint32_t d=0; d<depth; ++d){
            #         indent << INDENT_STR;
            #     }
            # }
            indent = ""
            if self.GetIndentSubreports():
                indent = indent_str * depth

            # out << indent.str() << report_prefix_;
            out.write(indent)
            out.write(self.GetReportPrefix())

            # if(quote_report_names_){
            #     out << "\"";
            # }
            if self.GetQuoteReportNames():
                out.write("\"")

            # out << r->getName();
            out.write(report_name)

            # if(quote_report_names_){
            #     out << "\"";
            # }
            if self.GetQuoteReportNames():
                out.write("\"")

            # // Report range at top-level of the report
            # if(show_report_range_ && depth == 0){
            #     out << " [" << r->getStart() << ",";
            #     if(r->getEnd() == Scheduler::INDEFINITE){
            #         const Scheduler * sched = getScheduler(false);
            #         out << (sched ? sched->getCurrentTick() : r->getEnd());
            #     }else{
            #         out << r->getEnd();
            #     }
            #     out << "]";
            # }
            # out << "\n";
            if self.GetShowReportRange() and depth == 0:
                out.write(" [")
                out.write(str(report_start))
                out.write(",")
                out.write(str(report_end))
                out.write("]")
            out.write("\n")

            # indent << INDENT_STR << ADDITIONAL_STAT_INDENT;
            indent += indent_str + additional_stat_indent

            # uint32_t val_col_after_indent = val_col_;
            # if(val_col_after_indent >= indent.str().size()){
            #     val_col_after_indent -= indent.str().size();
            # }
            val_col_after_indent = self.GetValueColumn()
            if val_col_after_indent >= len(indent):
                val_col_after_indent -= len(indent)

            # for(const statistics::stat_pair_t& si : r->getStatistics()){
            report_stats = self.__GetReportStatistics(db_conn, report_id)
            for si in report_stats:
                si_name = si.GetName()
                if not si_name:
                    si_name = si.GetLocation()

                # out << indent.str();
                # std::stringstream name;
                # if(val_col_ > 0){
                #     name << std::left << std::setfill(' ') << std::setw(val_col_after_indent);
                # }
                #
                # // Generate Stat Name
                # if(si.first != ""){
                #     // Print name
                #     name << si.first;
                # }else{
                #     // Print location = value
                #     name << si.second->getLocation();
                # }
                # name << " = ";
                out.write(indent)
                if self.GetValueColumn() > 0:
                    name = f"{si_name:<{val_col_after_indent}}"
                else:
                    name = si_name
                name += " = "

                # // Generate Value
                # double val = si.second->getValue();
                # name << Report::formatNumber(val);
                val = si.GetValue()
                name += FormatNumber(val)

                # // Print description column
                # TODO cnyce: Fill in this code!
                assert self.GetShowDescriptions() == False, "Description column not implemented yet"

                # // Write line to the output
                # out << name.str() << std::endl;
                out.write(name)
                out.write("\n")

            # // Print newline if any stats were printed
            # if(r->getStatistics().size() > 0){
            #     out << std::endl;
            # }
            if len(report_stats) > 0:
                out.write("\n")

            # for(const Report& sr : r->getSubreports()){
            #     dump_(out, &sr, depth+1);
            # }
            subrep_ids = self.__GetSubreports(db_conn, descriptor_id, report_id)
            for subrep_id in subrep_ids:
                self.__Dump(db_conn, out, descriptor_id, subrep_id, depth + 1)

            # // Print addtional newline after the subreports if any stats were
            # // printed at this level
            # if(r->getSubreports().size() > 0){
            #     out << std::endl;
            # }
            if len(subrep_ids) > 0:
                out.write("\n")

    def __HasStatistics(self, db_conn, report_id):
        cursor = db_conn.cursor()
        cmd = "SELECT COUNT(*) FROM StatisticInsts WHERE ReportID = ?"
        cursor.execute(cmd, (report_id,))
        count = cursor.fetchone()[0]
        return count > 0

    def __GetReportStatistics(self, db_conn, report_id):
        cursor = db_conn.cursor()
        cmd = "SELECT StatisticName, StatisticLoc, StatisticDesc FROM StatisticInsts WHERE ReportID = ?"
        cursor.execute(cmd, (report_id,))

        statistics = []
        for name, loc, desc in cursor.fetchall():
            statistics.append(StatisticInstance(name, loc, desc, self.stat_value_getter))

        return statistics

    def __GetSubreports(self, db_conn, descriptor_id, parent_report_id):
        cmd = f"SELECT Id FROM Reports WHERE ReportDescID = {descriptor_id} AND ParentReportID = {parent_report_id}"
        cursor = db_conn.cursor()
        cursor.execute(cmd)

        subrep_ids = []
        for row in cursor.fetchall():
            subrep_ids.append(row[0])

        return subrep_ids
