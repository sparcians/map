const Colors = {
    WARNING: '#ffffb0',
    GOOD: '#b0ffb0',
    ERROR: '#ffb0b0',
    NEUTRAL: '#e0e0e0',
}

function latency_to_color(latency) {
    if (latency < 300) {
        return Colors.NEUTRAL
    } else if (latency < 1000) {
        return Colors.WARNING
    } else {
        return Colors.ERROR
    }
}

const WidgetDataFiltering = {
    BP: 'bp',
    NONE: 'none',
}

const Processors = {
    SHP_LINE_PLOT: 'shp-line-plot-generator',
    SHP_BRANCH_LIST: 'shp-branch-list-generator',
    SHP_HEATMAP: 'shp-heatmap-generator',
    SHP_BRANCH_PROFILE: 'shp-branch-profile-generator',
    SIMDB_LINE_PLOT: 'simdb-line-plot-generator',
    SIMDB_CORRELATION_DATA: 'simdb-get-all-data-generator',
    PEVENT_TRACE: 'pevent-trace-generator'
}

const StatTypes = {
    STAT_TYPE_SUMMABLE: 'delta',
    STAT_TYPE_COUNTER: 'counter',
    STAT_TYPE_ATTRIBUTE: 'attribute',
}

const DataTypes = {
    SIMDB: 'sparta-statistics',
    SHP_TRAINING_TRACE: 'branch-predictor-training-trace',
    PEVENT_TRACE: 'pevent-trace'
}

const DataTypesShorthands = {
    [DataTypes.SIMDB]: 'sparta',
    [DataTypes.SHP_TRAINING_TRACE]: 'bp',
}

// Get a shorthand
function get_data_type_shorthand(dt) {
    const s = DataTypesShorthands[dt]
    if (typeof s != 'undefined')
        return s

    return dt
}

const STAT_NAME_SEPARATOR = '.'

const ColorPalettes= [[
        '#000003',
        '#01010B',
        '#040415',
        '#0A0722',
        '#0F0B2C',
        '#160E3A',
        '#1C1046',
        '#231152',
        '#2D1060',
        '#350F6A',
        '#400F73',
        '#481078',
        '#52127C',
        '#5A157E',
        '#61187F',
        '#6B1C80',
        '#731F81',
        '#7C2381',
        '#842681',
        '#8D2980',
        '#952C80',
        '#9E2E7E',
        '#A7317D',
        '#B0347B',
        '#B93778',
        '#C23A75',
        '#CB3E71',
        '#D3426D',
        '#DA4769',
        '#E24D65',
        '#E85461',
        '#EE5D5D',
        '#F3655C',
        '#F6705B',
        '#F9795C',
        '#FA825F',
        '#FC8E63',
        '#FD9768',
        '#FDA26F',
        '#FEAC75',
        '#FEB77D',
        '#FEC085',
        '#FEC98D',
        '#FDD497',
        '#FDDD9F',
        '#FCE8AA',
        '#FCF1B3',
        '#FBFCBF'],
        ['#440154',
        '#45085B',
        '#470F62',
        '#47186A',
        '#481E70',
        '#472676',
        '#472C7B',
        '#45327F',
        '#433A83',
        '#424085',
        '#3F4788',
        '#3D4C89',
        '#3A538B',
        '#37588C',
        '#355D8C',
        '#32638D',
        '#30688D',
        '#2D6E8E',
        '#2B738E',
        '#29798E',
        '#277D8E',
        '#25828E',
        '#23888D',
        '#218C8D',
        '#1F928C',
        '#1E978A',
        '#1E9C89',
        '#1FA187',
        '#21A685',
        '#25AB81',
        '#2AB07E',
        '#32B57A',
        '#39B976',
        '#44BE70',
        '#4DC26B',
        '#57C665',
        '#64CB5D',
        '#70CE56',
        '#7ED24E',
        '#8BD546',
        '#9AD83C',
        '#A7DB33',
        '#B5DD2B',
        '#C5DF21',
        '#D2E11B',
        '#E1E318',
        '#EEE51B',
        '#FDE724'],
        ['#00204C',
        '#002355',
        '#00275D',
        '#002B68',
        '#002E6F',
        '#00326E',
        '#06366E',
        '#17396D',
        '#243E6C',
        '#2C416B',
        '#35466B',
        '#3B496B',
        '#424E6B',
        '#47516B',
        '#4C556B',
        '#52596C',
        '#575D6D',
        '#5D616E',
        '#61656F',
        '#676970',
        '#6B6D71',
        '#6F7073',
        '#757575',
        '#797877',
        '#7E7D78',
        '#838178',
        '#898578',
        '#8E8978',
        '#938D78',
        '#999277',
        '#9E9676',
        '#A49B75',
        '#A99F74',
        '#AFA473',
        '#B5A871',
        '#BAAC6F',
        '#C0B16D',
        '#C6B66B',
        '#CCBB68',
        '#D2C065',
        '#D8C561',
        '#DECA5E',
        '#E4CE5B',
        '#EBD456',
        '#F0D951',
        '#F7DF4B',
        '#FDE345',
        '#FFE945'],]