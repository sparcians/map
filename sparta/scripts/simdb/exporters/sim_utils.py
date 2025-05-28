from collections import OrderedDict

class SimulationInfo:
    @classmethod
    def GetHeaderPairs(cls, db_conn):
        cursor = db_conn.cursor()
        cmd = "SELECT HeaderName, HeaderValue FROM SimulationInfoHeaderPairs WHERE HeaderName != 'Other'"
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

        # if(other.size() > 0){
        #     o << line_start << "Other:" << line_end;
        #     for(const std::string& oi : other){
        #         o << line_start << "  " << oi << line_end;
        #     }
        # }
        cmd = "SELECT HeaderValue FROM SimulationInfoHeaderPairs WHERE HeaderName == 'Other'"
        cursor = db_conn.cursor()
        cursor.execute(cmd)

        for row in cursor.fetchall():
            out.write(f"{line_start}Other:  {row[0]}{line_end}")

def GetSimInfo(cursor):
    cmd = "SELECT SimName, SimVersion, SpartaVersion, ReproInfo FROM SimulationInfo"
    cursor.execute(cmd)

    sim_name, sim_version, sparta_version, repro_info = cursor.fetchone()
    json_version = "2.1"
    siminfo = OrderedDict([
        ("name", sim_name),
        ("sim_version", sim_version),
        ("sparta_version", sparta_version),
        ("json_report_version", json_version),
        ("reproduction", repro_info)
    ])

    return siminfo

def GetVisibilities(cursor):
    cmd = "SELECT Hidden, Support, Detail, Normal, Summary, Critical FROM Visibilities"
    cursor.execute(cmd)
    hidden, support, detail, normal, summary, critical = cursor.fetchone()
    vis = OrderedDict([
        ("hidden", hidden),
        ("support", support),
        ("detail", detail),
        ("normal", normal),
        ("summary", summary),
        ("critical", critical)
    ])

    return vis
