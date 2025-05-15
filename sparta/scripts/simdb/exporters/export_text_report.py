from .sim_utils import *
from .stat_utils import *
import io
from .utils import FormatNumber

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
        default_show_descs = "true" if self.GetShowDescriptions() else "false"
        show_descs = GetReportStyle(cursor, report_id, descriptor_id, "show_descriptions", default_show_descs) == "true"

        # const std::string INDENT_STR = "  ";
        indent_str = "  "

        # const std::string ADDITIONAL_STAT_INDENT = "  ";
        additional_stat_indent = "  "

        # if(write_contentless_reports_ || hasStatistics_(r)){
        if self.GetWriteContentlessReports() or HasStatistics(db_conn.cursor(), report_id):
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
            report_stats = GetReportStatInsts(db_conn.cursor(), report_id, self.stat_value_getter)
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
                assert show_descs == False, "Description column not implemented yet"

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
            subrep_ids = GetSubreportIDs(db_conn.cursor(), descriptor_id, report_id)
            for subrep_id in subrep_ids:
                self.__Dump(db_conn, out, descriptor_id, subrep_id, depth + 1)

            # // Print additional newline after the subreports if any stats were
            # // printed at this level
            # if(r->getSubreports().size() > 0){
            #     out << std::endl;
            # }
            if len(subrep_ids) > 0:
                out.write("\n")
