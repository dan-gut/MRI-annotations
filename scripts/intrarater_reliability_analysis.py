import numpy as np
import pandas as pd
import altair as alt
import matplotlib.pyplot as plt
import os

ANN_DIR = '/home/daniel/Pulpit/!FinalAnnotationsTask/annotations_intrarater_reliability_postprocessed'

RATERS = ['WadimWojciechowski', 'IwonaKucybala', 'KamilKrupa', 'MiloszRozynek']

SERIES = ['series I', 'series II', 'series III']

ANN_TYPES = ['KNEE1250LSC', 'KNEE2500LSC', 'KNEEMANUAL', 'SPA1000LSC', 'SPA2000LSC', 'SPAMANUAL']


def read_binary_data(filepath, img_size, slices_no, signed=False):
    if signed:
        img = np.zeros((img_size, img_size, slices_no), dtype=np.int8)
    else:
        img = np.zeros((img_size, img_size, slices_no), dtype=np.uint8)

    f = open(filepath, "rb")
    for i in range(slices_no):
        for j in range(img_size):
            for k in range(img_size):
                byte = f.read(1)
                val = byte[0]
                img[j, k, i] = val
    f.close()
    return img


def load_data():
    manual_annotations = {}
    combined_annotations = {}
    sp_annotations = {}
    
    for rater in RATERS:
        manual_annotations[rater] = {ann_type: {} for ann_type in ANN_TYPES}
        combined_annotations[rater] = {ann_type: {} for ann_type in ANN_TYPES}
        sp_annotations[rater] = {ann_type: {} for ann_type in ANN_TYPES if ann_type[-6:] != "MANUAL"}

    for rater in os.listdir(ANN_DIR):
        if rater in RATERS:
            for series in SERIES:
                for ann_type in (os.listdir(f'{ANN_DIR}/{rater}/{series}/sp')):
                    if ann_type in ANN_TYPES:
                        for case_filename in os.listdir(f'{ANN_DIR}/{rater}/{series}/sp/{ann_type}'):
                            if case_filename.endswith(".raw"):
                                _, case_no, img_size, _, slices_no, _, _ = case_filename.split('_')
                                img_size = int(img_size)
                                slices_no = int(slices_no)
                                case_no = int(case_no)

                                if case_no not in sp_annotations[rater][ann_type].keys():
                                    sp_annotations[rater][ann_type][case_no] = {}

                                sp_annotations[rater][ann_type][case_no][series] = \
                                    read_binary_data(f'{ANN_DIR}/{rater}/{series}/sp/{ann_type}/{case_filename}',
                                                     img_size, slices_no, signed=False)

                for ann_type in (os.listdir(f'{ANN_DIR}/{rater}/{series}/manual')):
                    if ann_type in ANN_TYPES:
                        for case_filename in os.listdir(f'{ANN_DIR}/{rater}/{series}/manual/{ann_type}'):
                            if case_filename.endswith(".raw"):
                                _, case_no, img_size, _, slices_no, _, _ = case_filename.split('_')
                                img_size = int(img_size)
                                slices_no = int(slices_no)
                                case_no = int(case_no)

                                if case_no not in manual_annotations[rater][ann_type].keys():
                                    manual_annotations[rater][ann_type][case_no] = {}

                                manual_annotations[rater][ann_type][case_no][series] = \
                                    read_binary_data(f'{ANN_DIR}/{rater}/{series}/manual/{ann_type}/{case_filename}',
                                                     img_size, slices_no, signed=True)

                                if case_no not in combined_annotations[rater][ann_type].keys():
                                    combined_annotations[rater][ann_type][case_no] = {}

                                if ann_type[-6:] != "MANUAL":
                                    combined_annotations[rater][ann_type][case_no][series] = \
                                        sp_annotations[rater][ann_type][case_no][series] + \
                                        manual_annotations[rater][ann_type][case_no][series]

                                    assert np.max(sp_annotations[rater][ann_type][case_no][series]) in (0, 1), \
                                        f'SP max value incorrect for {rater}, {series}, {ann_type}, {case_no}'
                                    assert np.min(sp_annotations[rater][ann_type][case_no][series]) in (0, 1), \
                                        f'SP min value incorrect for {rater}, {series}, {ann_type}, {case_no}'

                                    assert np.max(manual_annotations[rater][ann_type][case_no][series]) in (-1, 0, 1), \
                                        f'Manual max value incorrect for {rater}, {series}, {ann_type}, {case_no}'
                                    assert np.min(manual_annotations[rater][ann_type][case_no][series]) in (-1, 0, 1), \
                                        f'Manual min value incorrect for {rater}, {series}, {ann_type}, {case_no}'

                                    assert np.max(combined_annotations[rater][ann_type][case_no][series]) in (0, 1), \
                                        f'Combined max value incorrect for {rater}, {series}, {ann_type}, {case_no}'
                                    assert np.max(combined_annotations[rater][ann_type][case_no][series]) in (0, 1), \
                                        f'Combined min value incorrect for {rater}, {series}, {ann_type}, {case_no}'
                                else:
                                    combined_annotations[rater][ann_type][case_no][series] = \
                                        manual_annotations[rater][ann_type][case_no][series]

                                    assert np.max(manual_annotations[rater][ann_type][case_no][series]) in (0, 1), \
                                        f'Manual max value incorrect for {rater}, {series}, {ann_type}, {case_no}'
                                    assert np.min(manual_annotations[rater][ann_type][case_no][series]) in (0, 1), \
                                        f'Manual min value incorrect for {rater}, {series}, {ann_type}, {case_no}'

    for rater in RATERS:
            for ann_type in ANN_TYPES:  # remove empty dictionaries
                if not combined_annotations[rater][ann_type]:
                    del combined_annotations[rater][ann_type]
                if not manual_annotations[rater][ann_type]:
                    del manual_annotations[rater][ann_type]
                if ann_type[-6:] != "MANUAL":
                    if not sp_annotations[rater][ann_type]:
                        del sp_annotations[rater][ann_type]

    return manual_annotations, sp_annotations, combined_annotations


def manual_to_sp_ratio(manual_ann, sp_ann):
    if np.sum(sp_ann) == 0:
        return -1
    else:
        return np.sum(np.abs(manual_ann))/np.sum(sp_ann)


def agreement_area_ratio(ann_list):
    ann_sum = np.sum(ann_list, axis=0)

    vals, counts = np.unique(ann_sum, return_counts=True)
    total_ann = np.count_nonzero(ann_sum)

    if total_ann == 0:
        return 0  # nothing is annotated

    ratios = {1: 0., 2: 0., 3: 0.}

    for val, count in zip(vals, counts):
        if val == 0:
            continue
        ratios[val] = count/total_ann

    return ratios


def plot_agreement_ratios(agreement_area_ratios, description=''):
    agr_area_1 = {}
    agr_area_2 = {}
    agr_area_3 = {}

    for rater, ann_t_dict in agreement_area_ratios.items():
        if rater not in agr_area_1:
            agr_area_1[rater] = {}
        if rater not in agr_area_2:
            agr_area_2[rater] = {}
        if rater not in agr_area_3:
            agr_area_3[rater] = {}

        for ann_t, ratios_dict in ann_t_dict.items():
            agr_area_1[rater][ann_t] = agreement_area_ratios[rater][ann_t][1]
            agr_area_2[rater][ann_t] = agreement_area_ratios[rater][ann_t][2]
            agr_area_3[rater][ann_t] = agreement_area_ratios[rater][ann_t][3]

    def prep_df(df, name):
        df = df.stack().reset_index()
        df.columns = ['method', 'rater', 'ratios']
        df['Agreement area'] = name
        return df

    chart_data_1 = prep_df(pd.DataFrame(agr_area_1), '1 annotation')
    chart_data_2 = prep_df(pd.DataFrame(agr_area_2), '2 annotations')
    chart_data_3 = prep_df(pd.DataFrame(agr_area_3), '3 annotations')
    chart_data = pd.concat([chart_data_1, chart_data_2, chart_data_3])

    chart = alt.Chart(chart_data).mark_bar().encode(
        x=alt.X('method:N', title=None),  # field to group columns on
        y=alt.Y('sum(ratios):Q', axis=alt.Axis(grid=True, title=None)),  # field to use as Y values and how to calculate
        column=alt.Column('rater:N', title=None),  # field to use as the set of columns to be  represented in each group
        color=alt.Color('Agreement area:N',  # field to use for color segmentation
                        scale=alt.Scale(range=['#2ca02c', '#ff7f0e', '#d62728'], ),  # color pallet
                        )) \
        .configure_view(strokeOpacity=0)  # remove grid lines around column clusters

    # import altair_viewer
    # altair_viewer.show(chart)
    chart.save(f'plots/intrarater_agreement_area_{description}.png')


def plot_correction_ratio(manual_to_sp_ratios, description=''):
    pd.DataFrame(manual_to_sp_ratios).plot(kind='bar', title=f'Manual_corrections/superpixel_{description}')
    plt.savefig(f'plots/intrarater_manual_to_sp_ratio_{description}.png')
    plt.clf()


if __name__ == '__main__':
    manual_annotations, sp_annotations, combined_annotations = load_data()

    manual_to_sp_ratios_SPA = {}
    manual_to_sp_ratios_KNEE = {}
    agr_area_ratios_SPA = {}
    agr_area_ratios_KNEE = {}

    m2s_ratios = {ann_type: {series: [] for series in SERIES} for ann_type in ANN_TYPES if ann_type[-6:] != "MANUAL"}
    for rater, ann_data in manual_annotations.items():
        agr_area_ratios_SPA[rater] = {}
        agr_area_ratios_KNEE[rater] = {}

        agr_ratios = {ann_type: [] for ann_type in ANN_TYPES}
        for ann_type, cases in ann_data.items():
            if ann_type[-6:] != "MANUAL":
                if ann_type[:3] == 'SPA' and ann_type not in manual_to_sp_ratios_SPA:
                    manual_to_sp_ratios_SPA[ann_type] = {}
                elif ann_type[:4] == 'KNEE' and ann_type not in manual_to_sp_ratios_KNEE:
                    manual_to_sp_ratios_KNEE[ann_type] = {}

                ratios = {series: [] for series in SERIES}
                for case_no, series_data in cases.items():
                    for series in series_data.keys():
                        ratio = manual_to_sp_ratio(manual_annotations[rater][ann_type][case_no][series],
                                                   sp_annotations[rater][ann_type][case_no][series])
                        if ratio != -1:
                            ratios[series].append(ratio)

                for series, ratios in ratios.items():
                    m2s_ratios[ann_type][series].append(np.mean(ratios))

            for case_no, series_data in cases.items():
                agr_ratios[ann_type].append(agreement_area_ratio(
                    list(combined_annotations[rater][ann_type][case_no].values())))

        for ann_type, ratios_list in agr_ratios.items():
            mean_ratio = {}
            for ratio_no in (1, 2, 3):
                curr_ratios = [case_ratios[ratio_no] for case_ratios in ratios_list]
                mean_ratio[ratio_no] =  np.mean(curr_ratios)

            if ann_type[:3] == 'SPA':
                agr_area_ratios_SPA[rater][ann_type] = {ratio_no: mean_ratio[ratio_no] for ratio_no in (1, 2, 3)}
            elif ann_type[:4] == 'KNEE':
                agr_area_ratios_KNEE[rater][ann_type] = {ratio_no: mean_ratio[ratio_no] for ratio_no in (1, 2, 3)}

    for ann_type, series_data in m2s_ratios.items():
        for series_no, ratios in series_data.items():
            if ann_type[:3] == 'SPA':
                manual_to_sp_ratios_SPA[ann_type][series_no] = np.mean(ratios)
            elif ann_type[:4] == 'KNEE':
                manual_to_sp_ratios_KNEE[ann_type][series_no] = np.mean(ratios)

    agr_area_ratios_SPA['All'] = {}
    agr_area_ratios_KNEE['All'] = {}
    all_ratios_SPA = {}
    for rater, ann_t_data in agr_area_ratios_SPA.items():
        for ann_type, ratios_data in ann_t_data.items():
            if ann_type not in all_ratios_SPA.keys():
                all_ratios_SPA[ann_type] = {ratio_no: [] for ratio_no in (1, 2, 3)}
            for ratio_no, ratio_value in ratios_data.items():
                all_ratios_SPA[ann_type][ratio_no].append(ratio_value)

    all_ratios_KNEE = {}
    for rater, ann_t_data in agr_area_ratios_KNEE.items():
        for ann_type, ratios_data in ann_t_data.items():
            if ann_type not in all_ratios_KNEE.keys():
                all_ratios_KNEE[ann_type] = {ratio_no: [] for ratio_no in (1, 2, 3)}
            for ratio_no, ratio_value in ratios_data.items():
                all_ratios_KNEE[ann_type][ratio_no].append(ratio_value)

    for ann_type, ratios_data in all_ratios_SPA.items():
        agr_area_ratios_SPA['All'][ann_type] = {ratio_no: np.mean(ratios_data[ratio_no]) for ratio_no in (1, 2, 3)}
    for ann_type, ratios_data in all_ratios_KNEE.items():
        agr_area_ratios_KNEE['All'][ann_type] = {ratio_no: np.mean(ratios_data[ratio_no]) for ratio_no in (1, 2, 3)}

    plot_agreement_ratios(agr_area_ratios_SPA, description='SPA')
    plot_correction_ratio(manual_to_sp_ratios_SPA, description='SPA')
    plot_agreement_ratios(agr_area_ratios_KNEE, description='KNEE')
    plot_correction_ratio(manual_to_sp_ratios_KNEE, description='KNEE')
